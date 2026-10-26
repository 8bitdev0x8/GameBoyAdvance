"""Bounded RP2350 build/flash/capture trials. No firmware self-modification.

All hardware writes require a pinned Feather USB serial and a validated fallback.
Trials leave the baseline running. Timed boot windows are observations, not replay.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from dataclasses import asdict
from datetime import datetime
import hashlib
import json
import os
import re
from pathlib import Path
import statistics
import struct
import subprocess
import sys
import time

import benchmark_native as bench
import debug_log
from debug_charts import write_report
import rom_uploader_backend as uploader
import video_control

VERSION = "1.0.0"
PROJECT = Path(__file__).resolve().parents[1]


def local_time():
    return datetime.now().astimezone().isoformat()


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


@contextmanager
def lab_lock(path):
    """OS releases the lock on crashes; retain the file to avoid inode races."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as handle:
        if os.fstat(handle.fileno()).st_size == 0:
            handle.write(b"0")
            handle.flush()
        handle.seek(0)
        try:
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise RuntimeError("Another performance-lab run holds the board lock") from error
        try:
            yield
        finally:
            handle.seek(0)
            if os.name == "nt":
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(handle, fcntl.LOCK_UN)


def artifact(firmware, profile_map, directory):
    firmware, profile_map = Path(firmware).resolve(), Path(profile_map).resolve()
    layout, flash = uploader.validate_uf2(firmware)
    records, fingerprint = debug_log.map_records(profile_map)
    table = b"".join(struct.pack("<II24s", r["begin"], r["end"], r["label"].encode("ascii")) for r in records)
    if not records or flash.count(table) != 1:
        raise ValueError("Profile-map bytes are not uniquely present in this UF2")
    return dict(firmware=str(firmware), profile_map=str(profile_map), fingerprint=fingerprint,
                layout=layout, output=str(directory), uploaded=False)


def resolve_target(serial_number, preferred_port=None):
    matches = [d for d in uploader.devices() if d.supported and d.serial == serial_number]
    if len(matches) != 1:
        raise RuntimeError("Exactly one Feather with the pinned USB serial must be connected")
    target = matches[0]
    if preferred_port and preferred_port.casefold() != target.port.casefold():
        print(f"Pinned Feather moved from {preferred_port} to {target.port}", flush=True)
    return target


def health_reasons(header, frames, profiles, expected_map=None, engine="interpreter"):
    if engine not in ("interpreter","flash-aot"):
        raise ValueError("Unsupported expected CPU engine")
    summary = debug_log.summarize(header, frames, profiles)
    issues = []
    if not frames:
        return ["No completed frames"]
    if expected_map is not None and header["map_fingerprint"] != expected_map:
        issues.append("Runtime PC map differs from the flashed artifact")
    if summary["invalid_frame_partitions"] or summary["frame_number_gaps"]:
        issues.append("Invalid timing partitions or discontinuous frames")
    expected_native=engine=="flash-aot"
    if any(f["native_on"]!=expected_native or f["blocks_on"] or f["alu_burst_on"] for f in frames):
        issues.append("CPU engine flags differ from the requested " + engine)
    if expected_native and (not header["flags"] & 2048 or any(not f["flags"] & 2048 for f in frames)):
        issues.append("Flash AOT identity flag missing; ordinary native mode is not an AOT trial")
    wall = sum(f["wall_us"] for f in frames)
    if not wall:
        issues.append("Zero elapsed frame time")
    else:
        ring=bool(header["flags"] & 1024)
        low,high=(45,75) if ring else (25000,38000)
        if any(bool(f["flags"] & 1024)!=ring for f in frames):
            issues.append("Header/frame HDMI backend differs")
        if not low <= sum(f["hdmi_irqs"] for f in frames) * 1e6 / wall <= high:
            issues.append("Abnormal HDMI IRQ rate")
    if len({f["hdmi_resets"] for f in frames}) != 1:
        issues.append("HDMI restarted during capture")
    if min(f["free_heap_cached"] for f in frames) == 0:
        issues.append("Runtime reports no cached free heap")
    return issues


def save_report(data, stem, profile_map=None, expected_map=None, engine="interpreter"):
    stem.with_suffix(".bin").write_bytes(data)
    header, frames, profiles = debug_log.decode(data, profile_map)
    summary = debug_log.summarize(header, frames, profiles)
    issues = health_reasons(header, frames, profiles, expected_map, engine)
    summary.update(capture_local_time=local_time(), health_issues=issues, expected_engine=engine,
                   capture_sha256=hashlib.sha256(data).hexdigest(),
                   frame_keys=sorted({f["keys"] for f in frames}),
                   pad_on_frames=sum(f["pad_on"] for f in frames), frames=len(frames),
                   irq_per_second=sum(f["hdmi_irqs"] for f in frames) / max(sum(f["wall_us"] for f in frames), 1) * 1e6)
    if frames:
        mean = summary["mean_ms_per_frame"]
        summary["run_outside_wait_ms"] = mean["run"] - mean["wait"]
        summary["p95_frame_ms"] = sorted(f["wall_ms"] for f in frames)[max(0, (95 * len(frames) + 99) // 100 - 1)]
    debug_log.write_csv(stem.with_name(stem.name + "-frames.csv"), frames, debug_log.FRAME_FIELDS)
    debug_log.write_csv(stem.with_name(stem.name + "-profiles.csv"), profiles, debug_log.PROFILE_FIELDS)
    write_json(stem.with_name(stem.name + "-summary.json"), summary)
    write_report(stem.with_suffix(".html"), header, frames, profiles, summary)
    return summary


def query_video(device, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        device.write(b"cancel\nvideo status\n")
        until = min(deadline, time.monotonic() + 1)
        while time.monotonic() < until:
            line = device.readline().decode("ascii", errors="replace")
            if line.startswith("VID "):
                return video_control.parse_status(line)
    raise TimeoutError("No live video status; application may not have booted")


def connect(serial_number, timeout=90):
    import serial
    deadline, last = time.monotonic() + timeout, "USB port absent"
    while time.monotonic() < deadline:
        device = None
        try:
            target = resolve_target(serial_number)
            device = serial.Serial(target.port, 115200, timeout=.2, write_timeout=2)
            device.reset_input_buffer()
            video = query_video(device, min(5, max(1, deadline - time.monotonic())))
            return device, target, video
        except (OSError, RuntimeError, TimeoutError) as error:
            last = str(error)
            if device is not None:
                device.close()
            time.sleep(.5)
    raise TimeoutError(f"Feather did not become responsive: {last}")


def capture(serial_number, stem, seconds=20, warmup=0, candidate=None, timeout=90):
    device, target, initial = connect(serial_number, timeout)
    with device:
        if warmup:
            print(f"Warmup {warmup:g}s", flush=True)
            bench.wait_seconds(warmup)
        start = query_video(device)
        if seconds:
            # Clear at a command/status boundary, then allow uninterrupted recording.
            device.write(b"clear\n")
            bench.query_status(device)
            print(f"Recording {seconds:g}s on {target.port}", flush=True)
            bench.wait_seconds(seconds)
        data = bench.read_dump(device, timeout=45)
        end = query_video(device)
    raw = debug_log.read_header(data[:64])
    profile_map = Path(candidate["profile_map"]) if candidate else debug_log.matching_map(raw["map_fingerprint"], PROJECT / "output")
    result = save_report(data, stem, profile_map, candidate["fingerprint"] if candidate else None)
    result.update(port=target.port, serial=target.serial, video_before=start, video_after=end,
                  requested_seconds=seconds, warmup_seconds=warmup)
    if end["frames"] <= start["frames"] and seconds:
        result["health_issues"].append("Guest frame count did not advance")
    if end["restarts"] != start["restarts"]:
        result["health_issues"].append("Video restart counter changed across measurement")
    if seconds and result["retained_wall_seconds"] < seconds * .8:
        result["health_issues"].append("Less than 80% of the requested interval retained")
    write_json(stem.with_name(stem.name + "-summary.json"), result)
    print(f"{result.get('weighted_fps', 0):.2f} FPS; issues: {result['health_issues']}", flush=True)
    return result


def flash(item, target, directory):
    item = dict(item, output=str(directory))
    directory.mkdir(parents=True, exist_ok=False)
    with (directory / "upload.jsonl").open("w", encoding="utf-8") as log:
        def emit(kind, **fields):
            row = dict(at=local_time(), event=kind, **fields)
            log.write(json.dumps(row) + "\n")
            log.flush()
            if kind == "stage":
                print(fields.get("text", kind), flush=True)
        try:
            target = resolve_target(target.serial)
        except RuntimeError:
            pass  # BOOTSEL may expose no runtime COM port; picotool still filters serial.
        uploader.flash_firmware(item, target, uploader.Runner(emit))
    return item


def compare(phases):
    a, b, c = phases
    base_fps = (a["frames"] + c["frames"]) / (a["retained_wall_seconds"] + c["retained_wall_seconds"])
    drift = abs(a["weighted_fps"] - c["weighted_fps"]) / statistics.mean([a["weighted_fps"], c["weighted_fps"]]) * 100
    warnings = ["Timed boot windows are not deterministic game/input replays; scene confirmation is required"]
    if drift > 10:
        warnings.append("A1/A2 FPS drift exceeds 10%")
    if len({(p["cpu_hz"], p["psram_hz"]) for p in phases}) != 1:
        warnings.append("Clocks changed")
    if len({tuple(p["frame_keys"]) for p in phases}) != 1 or any(len(p["frame_keys"]) != 1 for p in phases):
        warnings.append("Input changed")
    if len({p["pad_on_frames"] == p["frames"] for p in phases}) != 1:
        warnings.append("Controller connection differs")
    if any(p["pad_on_frames"] not in (0, p["frames"]) for p in phases):
        warnings.append("Controller connection changed within a phase")
    return dict(baseline_weighted_fps=base_fps, candidate_fps=b["weighted_fps"],
                observed_change_percent=(b["weighted_fps"] / base_fps - 1) * 100,
                baseline_drift_percent=drift, warnings=warnings, promote=False,
                interpretation="Observation only; no automatic performance acceptance")


def run_board_trial(candidate, baseline, target, directory, seconds, warmup, smoke=False,
                    do_flash=flash, do_capture=capture):
    manifest = dict(version=VERSION, started_local=local_time(), target=asdict(target),
                    candidate=candidate, baseline=baseline, phases=[], completed=False,
                    baseline_restored=False, scope="Timed boot observations; no game-state replay")
    attempted = False
    try:
        phases = [("candidate", candidate)] if smoke else [("a1-baseline", baseline), ("b-candidate", candidate), ("a2-baseline", baseline)]
        for name, item in phases:
            attempted = True  # Reset/load can fail after changing device state.
            do_flash(item, target, directory / (name + "-upload"))
            result = do_capture(target.serial, directory / name, seconds, warmup, item)
            manifest["phases"].append(dict(name=name, **result))
            write_json(directory / "trial.json", manifest)
            if result["health_issues"]:
                raise RuntimeError("Health gate failed: " + "; ".join(result["health_issues"]))
        if not smoke:
            manifest["comparison"] = compare(manifest["phases"])
            manifest["baseline_restored"] = True
        manifest["completed"] = True
    except BaseException as error:
        manifest["error"] = f"{type(error).__name__}: {error}"
        write_json(directory / "trial.json", manifest)
        raise
    finally:
        # A smoke test of the known baseline can leave that exact image running.
        smoke_is_baseline = smoke and candidate["layout"]["sha256"] == baseline["layout"]["sha256"] and manifest["completed"]
        if smoke_is_baseline:
            manifest["baseline_restored"] = True
        if attempted and not manifest["baseline_restored"]:
            try:
                do_flash(baseline, target, directory / "restore-baseline-upload")
                restored = do_capture(target.serial, directory / "restore-baseline", 3, 2, baseline)
                if restored["health_issues"]:
                    raise RuntimeError("Fallback health gate failed: " + "; ".join(restored["health_issues"]))
                manifest["baseline_restored"] = True
            except BaseException as error:
                manifest["restore_error"] = f"{type(error).__name__}: {error}"
                print("Recovery needs attention. If USB is unresponsive, hold BOOT, tap RESET, release BOOT.", flush=True)
        manifest["finished_local"] = local_time()
        write_json(directory / "trial.json", manifest)
    return manifest


def source_digest():
    digest = hashlib.sha256()
    for path in sorted((PROJECT / "GBAFeather").rglob("*")):
        if path.is_file():
            digest.update(path.relative_to(PROJECT).as_posix().encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()


def host_checks(rom, bios, directory):
    prepared = (PROJECT / "GBAFeather/src/generated/LocalBios.h").read_text(encoding="utf-8")
    body = prepared.split("local_gba_bios_bin[] = {", 1)[1].split("};", 1)[0]
    prepared_bios = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", body))
    if prepared_bios != Path(bios).read_bytes():
        raise ValueError("Correctness-test BIOS differs from the prepared firmware BIOS")
    before = source_digest()
    env = {k: v for k, v in os.environ.items() if not k.startswith("GBA_TEST_")}
    env.update({"GBA_TEST_" + k: "1" for k in ["THREADED", "DIRECT_VIDEO", "RENDER_PROFILE", "AUTO_INPUT", "SLICED", "MUTE", "BIOS_VIEW"]})
    env.update(GBA_TEST_FRAMES="6000", GBA_TEST_BIOS_PATH=str(bios))
    build = PROJECT.parent / "build/gba-host"
    commands = [[sys.executable, str(PROJECT / "tests/prepare.py")],
                ["cmake", "--build", str(build), "--config", "Release", "--target", "gba_loader_test", "gba_core_audit"],
                [str(build / "Release/gba_core_audit.exe")],
                [str(build / "Release/gba_loader_test.exe"), str(rom), str(PROJECT / "GBAFeather/src/generated/romblocks.bin"), str(directory / "host-final.ppm")]]
    # A prepared matching packed ROM is required; the existing loader checks it
    # against the original .gba rather than accepting a different game's tests.
    for index, command in enumerate(commands):
        with (directory / f"check-{index}.log").open("w", encoding="utf-8") as output:
            subprocess.run(command, cwd=PROJECT.parent, env=env, stdout=output, stderr=subprocess.STDOUT, check=True, timeout=900,
                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    if before != source_digest():
        raise RuntimeError("Firmware source changed during correctness checks")
    evidence = dict(source_sha256=before, checked_local=local_time(), replay_frames=6000, passed=True)
    write_json(directory / "checks.json", evidence)
    return evidence


def main():
    pre = argparse.ArgumentParser(add_help=False)
    pre.add_argument("--config", type=Path, default=PROJECT / "perf-lab.local.json")
    config_args, _ = pre.parse_known_args()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=config_args.config)
    parser.add_argument("mode", choices=["capture", "smoke", "trial", "build"])
    parser.add_argument("--serial")
    parser.add_argument("--port")
    parser.add_argument("--candidate", type=Path)
    parser.add_argument("--map", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--baseline-map", type=Path)
    parser.add_argument("--rom", type=Path)
    parser.add_argument("--bios", type=Path)
    parser.add_argument("--seconds", type=float, default=20)
    parser.add_argument("--warmup", type=float, default=30)
    parser.add_argument("--label", default="unclassified")
    parser.add_argument("--log-dir", type=Path, default=PROJECT / "logs/perf-lab")
    if config_args.config.is_file():
        config = json.loads(config_args.config.read_text(encoding="utf-8"))
        allowed = {"serial", "port", "baseline", "baseline_map", "rom", "bios", "seconds", "warmup", "label"}
        if set(config) - allowed:
            parser.error("Unsupported local config fields: " + ", ".join(sorted(set(config) - allowed)))
        parser.set_defaults(**config)
    args = parser.parse_args()
    if not args.serial:
        parser.error("A pinned --serial or local serial configuration is required")
    if not 3 <= args.seconds <= 120 or not 0 <= args.warmup <= 600:
        parser.error("Use 3–120 measurement seconds and 0–600 warmup seconds")
    if args.mode in ("smoke", "trial", "build") and (not args.baseline or not args.baseline_map):
        parser.error("Hardware writes require --baseline and --baseline-map")
    if args.mode in ("smoke", "trial") and (not args.candidate or not args.map):
        parser.error("Prebuilt trials require --candidate and --map")
    if args.mode == "build" and (not args.rom or not args.bios):
        parser.error("Build/check/flash requires --rom and --bios for the prepared game")
    with lab_lock(PROJECT / "output/perf-lab.lock"):
        stamp = datetime.now().astimezone().strftime("%Y-%m-%d_%H-%M-%S-%f")
        directory = args.log_dir / (args.mode + "-" + stamp)
        directory.mkdir(parents=True, exist_ok=False)
        write_json(directory / "request.json", dict(version=VERSION, label=args.label, mode=args.mode, requested_local=local_time()))
        target = resolve_target(args.serial, args.port)
        if args.mode == "capture":
            capture(target.serial, directory / "capture", args.seconds, args.warmup)
        else:
            baseline = artifact(args.baseline, args.baseline_map, directory)
            candidate = artifact(args.candidate, args.map, directory) if args.candidate else None
            # Preserve the current ring before any reset. If the board is already
            # dead, require recovery instead of silently proceeding without it.
            capture(target.serial, directory / "before-reset", 0, 0)
            if args.mode == "build":
                checks = host_checks(args.rom, args.bios, directory)
                with (directory / "build.jsonl").open("w", encoding="utf-8") as output:
                    def emit(event, **fields):
                        output.write(json.dumps(dict(event=event, **fields)) + "\n"); output.flush()
                        if event == "stage": print(fields.get("text", event), flush=True)
                    built = uploader.build_rom(PROJECT, args.rom, uploader.Runner(emit))
                if checks["source_sha256"] != source_digest():
                    raise RuntimeError("Firmware source changed between checks and build")
                candidate = artifact(built["firmware"], built["profile_map"], directory)
            run_board_trial(candidate, baseline, target, directory, args.seconds, args.warmup, args.mode == "smoke")
        print("Results:", directory / ("capture.html" if args.mode == "capture" else "trial.json"), flush=True)


if __name__ == "__main__":
    main()
