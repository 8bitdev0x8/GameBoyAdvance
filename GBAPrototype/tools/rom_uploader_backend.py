"""Local ROM uploader service: JSON events for the Windows desktop application.

Each job snapshots the private emulator, builds in isolation, validates the UF2,
and optionally uploads to one explicitly selected Feather USB serial number.
"""
from __future__ import annotations
import argparse
from dataclasses import asdict, dataclass
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import shutil
import struct
import subprocess
import sys
import threading
import time

PROJECT = Path(__file__).resolve().parents[1]
APP_VERSIONS = json.loads((PROJECT / "app-versions.json").read_text(encoding="utf-8"))
FLASH_BYTES = 8 * 1024 * 1024
FEATHER_IDS = {(0x239A, 0x814F), (0x239A, 0xC14F)}
FAMILY_ARM = 0xE48BFF59
FAMILY_ABSOLUTE = 0xE48BFF57


class UploadError(RuntimeError):
    pass


class Cancelled(UploadError):
    pass


@dataclass(frozen=True)
class Device:
    port: str
    description: str
    serial: str
    vid: int | None
    pid: int | None
    supported: bool


def devices() -> list[Device]:
    from serial.tools import list_ports
    result = []
    for p in list_ports.comports():
        result.append(Device(p.device, p.description or p.device, p.serial_number or "",
            p.vid, p.pid, (p.vid, p.pid) in FEATHER_IDS and bool(p.serial_number)))
    return sorted(result, key=lambda p: (not p.supported, int(re.search(r"\d+", p.port)[0]) if re.search(r"\d+", p.port) else 999))


def inspect_rom(path: Path) -> dict:
    path = path.resolve(strict=True)
    size = path.stat().st_size
    if path.suffix.lower() != ".gba" or not 192 <= size <= 32 * 1024 * 1024:
        raise UploadError("Choose a .gba file between 192 bytes and 32 MiB.")
    raw = path.read_bytes()
    title = raw[0xA0:0xAC].rstrip(b"\x00 ").decode("ascii", errors="replace")
    title = "".join(c for c in title if c.isprintable()) or path.stem
    return dict(path=str(path), file=path.name, title=title, bytes=size,
        sha256=hashlib.sha256(raw).hexdigest(),
        header_checksum_valid=raw[0xBD] == ((-sum(raw[0xA0:0xBD]) - 0x19) & 255))


def validate_uf2(path: Path) -> tuple[dict, bytearray]:
    data = path.read_bytes()
    if not data or len(data) % 512:
        raise UploadError("The build did not produce a complete UF2 file.")
    flash = bytearray(b"\xff" * FLASH_BYTES)
    records = {}
    high = 0
    absolute = 0
    for offset in range(0, len(data), 512):
        h = struct.unpack_from("<8I", data, offset)
        magic0, magic1, flags, address, size, number, total, family = h
        if (magic0, magic1) != (0x0A324655, 0x9E5D5157) or struct.unpack_from("<I", data, offset + 508)[0] != 0x0AB16F30:
            raise UploadError("Invalid UF2 block signature.")
        if flags != 0x2000 or size != 256 or not total or number >= total:
            raise UploadError("Unsupported UF2 block layout.")
        if family == FAMILY_ABSOLUTE:
            # The installed picotool emits one architecture block with a
            # two-block transfer count, followed by an independently numbered
            # ARM image. Its preamble count is not the ARM image's block count.
            if address != 0x10FFFF00 or number != 0 or total not in (1, 2):
                raise UploadError("Invalid RP2350 architecture preamble.")
            absolute += 1
        elif family == FAMILY_ARM:
            if not 0x10000000 <= address < address + size <= 0x10000000 + FLASH_BYTES:
                raise UploadError("This game and emulator exceed the board's 8 MiB flash.")
            if number in records or (address - 0x10000000) % 256:
                raise UploadError("Repeated or unaligned UF2 flash block.")
            records[number] = (address, total)
            start = address - 0x10000000
            flash[start:start + size] = data[offset + 32:offset + 32 + size]
            high = max(high, start + size)
        else:
            raise UploadError("This firmware targets a different microcontroller.")
    if absolute != 1 or not records or set(records) != set(range(len(records))):
        raise UploadError("The RP2350 firmware contains missing blocks.")
    if any(total != len(records) for _, total in records.values()) or len({a for a, _ in records.values()}) != len(records):
        raise UploadError("The UF2 block count or address range is inconsistent.")
    if min(a for a, _ in records.values()) != 0x10000000:
        raise UploadError("The firmware has no flash boot header.")
    result = dict(uf2_bytes=len(data), flash_used_bytes=high, flash_capacity_bytes=FLASH_BYTES,
        sha256=hashlib.sha256(data).hexdigest(), arm_blocks=len(records), architecture_blocks=absolute)
    return result, flash


@dataclass(frozen=True)
class Tools:
    cli: Path
    config: Path
    python: Path
    picotool: Path

    @classmethod
    def discover(cls) -> "Tools":
        local = Path(os.environ.get("LOCALAPPDATA", str(Path.home() / "AppData/Local")))
        cli_candidates = [Path(os.environ.get("GBA_ARDUINO_CLI", "C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"))]
        if shutil.which("arduino-cli"):
            cli_candidates.append(Path(shutil.which("arduino-cli")))
        cli = next((p for p in cli_candidates if p.is_file()), None)
        config = Path.home() / ".arduinoIDE/arduino-cli.yaml"
        candidates = sorted((local / "Arduino15/packages/rp2040/tools/pqt-picotool").glob("*/picotool.exe"))
        python = Path(sys.executable)
        if python.name.lower() == "pythonw.exe":
            python = python.with_name("python.exe")
        if not cli or not config.is_file() or not candidates or not python.is_file():
            raise UploadError("Arduino IDE and the Arduino-Pico board tools are required. See ROM-UPLOADER.md for setup.")
        return cls(cli, config, python, candidates[-1])


class Runner:
    def __init__(self, emit, cancelled: threading.Event | None = None, frontend_version: str | None = None):
        self.emit = emit
        self.cancelled = cancelled or threading.Event()
        self.frontend_version = frontend_version

    def versions(self):
        return dict(backend=APP_VERSIONS["backend"], frontend=self.frontend_version,
            protocol=APP_VERSIONS["protocol"])

    def check(self):
        if self.cancelled.is_set():
            raise Cancelled("Build cancelled. The board was not flashed.")

    def run(self, args, *, cwd=None, timeout=1200, cancellable=True, log=True) -> str:
        if cancellable:
            self.check()
        creation = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
        process = subprocess.Popen([str(x) for x in args], cwd=cwd, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf-8", errors="replace",
            bufsize=1, creationflags=creation)
        lines = queue.Queue()
        def read():
            try:
                for line in process.stdout:
                    lines.put(line)
            finally:
                lines.put(None)
        reader = threading.Thread(target=read, daemon=True)
        reader.start()
        output = []
        deadline = time.monotonic() + timeout
        try:
            ended = False
            while not ended:
                if (cancellable and self.cancelled.is_set()) or time.monotonic() > deadline:
                    if os.name == "nt":
                        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                            capture_output=True, creationflags=creation, timeout=15)
                    # Some restricted Windows environments deny taskkill even
                    # for our own child. Still terminate the process we own.
                    if process.poll() is None:
                        process.kill()
                    process.wait(timeout=15)
                    if cancellable and self.cancelled.is_set():
                        raise Cancelled("Build cancelled. The board was not flashed.")
                    raise UploadError("The operation timed out. See the activity log.")
                try:
                    line = lines.get(timeout=0.1)
                except queue.Empty:
                    continue
                if line is None:
                    ended = True
                else:
                    output.append(line)
                    if log:
                        self.emit("log", text=line.rstrip())
            code = process.wait(timeout=15)
            if code:
                tail = "\n".join("".join(output).splitlines()[-12:])
                raise UploadError(f"Operation failed (exit {code}).\n{tail}")
            return "".join(output)
        finally:
            process.stdout.close()


def stage_project(project: Path, destination: Path):
    required = [project / "GBAFeather/src/core/cpu.cpp", project / "build.ps1"]
    if any(not p.is_file() for p in required):
        raise UploadError("The local emulator sources are missing. This uploader needs the complete private project.")
    destination.mkdir(parents=True, exist_ok=False)
    shutil.copytree(project / "GBAFeather", destination / "GBAFeather",
        ignore=shutil.ignore_patterns("__pycache__", "*.uf2", "*.elf"))
    selection = destination / "GBAFeather/BiosSelection.h"
    if selection.is_file():
        selection.write_text(selection.read_text(encoding="utf-8").replace("SONIC", "GBA"), encoding="utf-8")
    shutil.copytree(project / "linker", destination / "linker")
    (destination / "tools").mkdir()
    for name in ("prepare_bios.py", "prepare_pc_profile.py"):
        shutil.copy2(project / "tools" / name, destination / "tools" / name)
    shutil.copy2(project / "build.ps1", destination / "build.ps1")


def build_rom(project: Path, rom_path: Path, runner: Runner, tools: Tools | None = None) -> dict:
    runner.emit("stage", name="inspect", text="Checking your game")
    info = inspect_rom(rom_path)
    tools = tools or Tools.discover()
    runner.emit("rom", **info)
    runner.check()
    safe_name = re.sub(r"[^A-Za-z0-9_-]+", "-", rom_path.stem).strip("-")[:48] or "game"
    stamp = datetime.now().astimezone().strftime("%Y-%m-%d_%H-%M-%S-%f")
    job = project.parent / "build/rom-uploader" / f"{stamp}-{safe_name}"
    source = job / "project"
    output = project / "output/rom-uploader" / f"{stamp}-{safe_name}"
    runner.emit("stage", name="prepare", text="Preparing an isolated build")
    output.mkdir(parents=True, exist_ok=False)
    runner.output_directory = output
    runner.emit("workspace", output=str(output))
    stage_project(project, source)
    runner.check()
    runner.emit("stage", name="pack", text="Packing your game")
    runner.run([tools.python, project / "tools/pack_rom.py", info["path"], "--sketch", source / "GBAFeather"])
    generated = source / "GBAFeather/src/generated"
    packed = json.loads((generated / "manifest.json").read_text())
    if packed["sha256"] != info["sha256"]:
        raise UploadError("The game file changed while preparing it. Select it again.")
    if packed["packed_bytes"] >= FLASH_BYTES:
        raise UploadError("This game's packed data alone fills the 8 MiB flash. Choose a smaller game.")
    runner.emit("packed", bytes=packed["packed_bytes"], original_bytes=info["bytes"])
    runner.emit("stage", name="build", text="Building the emulator with your game")
    args = ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
        "-File", source / "build.ps1", "-Profile", "-DebugUsb", "-BuildDirectory", job / "build",
        "-OutputDirectory", output, "-CliPath", tools.cli, "-ConfigPath", tools.config,
        "-PythonPath", tools.python]
    if (generated / "LocalBios.h").is_file():
        args.append("-UsePreparedBios")
    runner.run(args, cwd=source)
    runner.check()
    runner.emit("stage", name="validate", text="Checking the firmware and embedded game")
    firmware = output / "GBAFeather.uf2"
    layout, flash = validate_uf2(firmware)
    link_map = (job / "build/GBAFeather.ino.map").read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^\s*(0x[0-9a-fA-F]+)\s+gba_rom_packed\s*$", link_map, re.M)
    if not match:
        raise UploadError("Could not verify the embedded game address.")
    rom_offset = int(match[1], 16) - 0x10000000
    payload = (generated / "romblocks.bin").read_bytes()
    if rom_offset < 0 or flash[rom_offset:rom_offset + len(payload)] != payload:
        raise UploadError("The built firmware does not contain the selected packed game exactly.")
    map_path = output / "pc-profile-map.json"
    shutil.copy2(generated / "pc-profile-map.json", map_path)
    # The ordinary log decoder discovers this new map without replacing any
    # historical versioned map or changing the working emulator's generated files.
    shutil.copy2(map_path, project / "output" / f"pc-profile-map-vrom-{stamp}.json")
    result = dict(versions=runner.versions(), rom=info, packed=packed, firmware=str(firmware), output=str(output),
        source_snapshot=str(source), layout=layout, built_local_time=datetime.now().astimezone().isoformat(),
        profile_map=str(map_path), bios="current prepared BIOS" if (generated / "LocalBios.h").is_file() else "MIT replacement BIOS",
        uploaded=False)
    (output / "build.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def flash_firmware(result: dict, target: Device, runner: Runner, tools: Tools | None = None):
    if not target.supported or not target.serial or (target.vid, target.pid) not in FEATHER_IDS:
        raise UploadError("Select the Feather RP2350 HSTX USB port, not the ESP32 or Bluetooth port.")
    tools = tools or Tools.discover()
    firmware = Path(result["firmware"])
    layout, _ = validate_uf2(firmware)
    if layout["sha256"] != result["layout"]["sha256"]:
        raise UploadError("The firmware changed after validation. Build it again.")
    current = next((d for d in devices() if d.port.casefold() == target.port.casefold()), None)
    if current and (current.serial, current.vid, current.pid) != (target.serial, target.vid, target.pid):
        raise UploadError("The selected COM port now belongs to a different device. Refresh and select your Feather again.")
    runner.check()
    runner.emit("stage", name="reset", text=f"Connecting to your Feather on {target.port}")
    reset_error = None
    if current:
        import serial
        try:
            with serial.Serial(target.port, 115200, timeout=0.2, write_timeout=1) as port:
                port.dtr = True
                port.write(b"cancel\nprepare flash\n")
                ready_deadline = time.monotonic() + 3
                prepared = False
                while time.monotonic() < ready_deadline:
                    if port.readline().strip() == b"FLASH READY":
                        prepared = True
                        break
                runner.emit("reset_prepare", acknowledged=prepared)
                port.baudrate = 1200
                port.dtr = False
        except (serial.SerialException, OSError) as error:
            # A successful 1200-baud touch can remove the Windows COM device
            # before reconfiguration returns. Probe this exact USB serial next.
            reset_error = str(error)
            runner.emit("reset_transition", text=reset_error)
    # Read-only probes first. Never fall back to uploading to every connected
    # RP-series board: USB serial filtering remains mandatory after the COM reset.
    deadline = time.monotonic() + 20
    last_error = ""
    while time.monotonic() < deadline:
        try:
            runner.run([tools.picotool, "info", "-d", "--ser", target.serial], timeout=8, cancellable=False, log=False)
            break
        except UploadError as error:
            last_error = str(error)
            time.sleep(0.5)
    else:
        raise UploadError("The selected Feather did not appear in upload mode. Close other serial programs; if USB is unresponsive hold BOOT, tap RESET, release BOOT.\n" + (reset_error or "") + "\n" + last_error)
    runner.emit("stage", name="flash", text="Writing and verifying your game — keep USB connected")
    runner.run([tools.picotool, "load", "-v", "-x", firmware, "--ser", target.serial],
        timeout=180, cancellable=False)
    result["uploaded"] = True
    result["uploaded_to"] = asdict(target)
    result["uploaded_local_time"] = datetime.now().astimezone().isoformat()
    result["upload_versions"] = runner.versions()
    Path(result["output"], "build.json").write_text(json.dumps(result, indent=2) + "\n")


def read_artifact(path: Path, project: Path = PROJECT) -> dict:
    path = path.resolve(strict=True)
    allowed = (project / "output/rom-uploader").resolve()
    if not path.is_relative_to(allowed) or path.name != "build.json":
        raise UploadError("Select a build produced by this ROM uploader.")
    result = json.loads(path.read_text())
    if Path(result["output"]).resolve() != path.parent or Path(result["firmware"]).resolve() != path.parent / "GBAFeather.uf2":
        raise UploadError("The saved build's paths are inconsistent. Build the game again.")
    layout, _ = validate_uf2(Path(result["firmware"]))
    if layout["sha256"] != result["layout"]["sha256"]:
        raise UploadError("The saved firmware has changed. Build the game again.")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", action="store_true", help="Print component and event protocol versions as JSON")
    parser.add_argument("--frontend-version", help="Calling desktop application's version; omitted for CLI use")
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument("--rom", type=Path)
    parser.add_argument("--flash", action="store_true")
    parser.add_argument("--artifact", type=Path, help="Retry upload using an existing build.json")
    parser.add_argument("--port")
    parser.add_argument("--serial")
    args = parser.parse_args()
    if args.version:
        print(json.dumps(APP_VERSIONS))
        return
    if args.frontend_version and not re.fullmatch(r"\d+\.\d+\.\d+", args.frontend_version):
        parser.error("--frontend-version must be major.minor.patch")
    if args.list_ports:
        print(json.dumps([asdict(d) for d in devices()]))
        return
    cancelled = threading.Event()
    def read_control():
        for line in sys.stdin:
            if line.strip() == "cancel":
                cancelled.set()
    threading.Thread(target=read_control, daemon=True).start()
    result = None
    log = []
    def emit(kind, **values):
        event = dict(event=kind, **values)
        log.append(event)
        print(json.dumps(event, ensure_ascii=True), flush=True)
    runner = Runner(emit, cancelled, args.frontend_version)
    emit("versions", **runner.versions())
    try:
        target = None
        if args.flash:
            target = next((d for d in devices() if d.port == args.port and d.serial == args.serial), None)
            if target is None and args.artifact:
                # A selected Feather may already be in BOOTSEL after a failed
                # handoff. Only a saved, previously verified identity is reused.
                saved = read_artifact(args.artifact).get("selected_target")
                if saved and saved["port"] == args.port and saved["serial"] == args.serial:
                    target = Device(**saved)
            if target is None or not target.supported:
                raise UploadError("Select a connected Feather RP2350 HSTX USB port.")
        if args.artifact:
            result = read_artifact(args.artifact)
        elif args.rom:
            result = build_rom(PROJECT, args.rom, runner)
        else:
            raise UploadError("Choose a .gba game first.")
        if target:
            result["selected_target"] = asdict(target)
            Path(result["output"], "build.json").write_text(json.dumps(result, indent=2) + "\n")
        emit("built", **result)
        if args.flash:
            flash_firmware(result, target, runner)
        emit("done", **result)
    except Exception as error:
        emit("error", text=str(error), cancelled=isinstance(error, Cancelled))
        raise SystemExit(2 if isinstance(error, Cancelled) else 1)
    finally:
        output = Path(result["output"]) if result else getattr(runner, "output_directory", None)
        if output:
            (output / "activity.jsonl").write_text("\n".join(json.dumps(e) for e in log) + "\n")


if __name__ == "__main__":
    main()
