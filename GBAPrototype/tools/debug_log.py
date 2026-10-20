"""Download/decode RP2350 v41+ binary frame logs. Decoding needs only Python stdlib."""
from __future__ import annotations
import argparse
import binascii
import csv
from datetime import datetime
import json
from pathlib import Path
import statistics
import struct
import time
import zlib

MAGIC = b"GBALOG41"
END = b"GBAEND41"
FRAME_FIELDS = (
    "frame,uptime_ms,wall_us,run_us,draw_us,copy_us,wait_us,qfull_us,barrier_us,"
    "prep_us,bg_us,obj_us,mix_us,rgb_us,other_us,worker_us,jobs,dirty_jobs,"
    "rom_misses,decode_us,ifetch_hits,ifetch_fills,guest_cycles,vblank_cycles,"
    "hdmi_irqs,hdmi_irq_us,guest_pc,flags,keys,hdmi_resets,free_heap_cached,key_reads"
).split(",")
PROFILE_FIELDS = (
    "frame,uptime_ms,window_ms,samples,bad,unmapped,handler_us,irq_samples,flags,"
    "id1,percent1,id2,percent2,id3,percent3,free_heap"
).split(",")
HEADER_FIELDS = (
    "payload_bytes,records,dropped_records,cpu_hz,psram_hz,map_fingerprint,"
    "dependent_sram_ns,dependent_psram32_ns,dependent_psram256_ns,flags,uptime_ms"
).split(",")
DEFAULT_LOG_DIR = Path(__file__).resolve().parents[1] / "logs"

def save_capture(data: bytes, requested: Path | None = None,
                 log_dir: Path = DEFAULT_LOG_DIR, at: datetime | None = None) -> Path:
    """Timestamp a capture in OS local time; never overwrite an earlier file."""
    requested = requested or Path("gba-log.bin")
    directory = log_dir if requested.parent == Path(".") else requested.parent
    directory.mkdir(parents=True, exist_ok=True)
    local = (at or datetime.now().astimezone()).astimezone()
    timestamp = local.strftime("%Y-%m-%d_%H-%M-%S") + f"-{local.microsecond // 1000:03d}"
    prefix = requested.stem if requested.suffix.lower() == ".bin" else requested.name
    stem = f"{prefix}-{timestamp}"
    collision = 0
    while True:
        path = directory / (stem + (f"-{collision:02d}" if collision else "") + ".bin")
        try:
            with path.open("xb") as output:
                output.write(data)
            return path
        except FileExistsError:
            collision += 1

def read_header(data: bytes) -> dict:
    if len(data) != 64 or data[:8] != MAGIC:
        raise ValueError("Not a v41 binary log header")
    if struct.unpack_from("<4H", data, 8) != (1, 64, 32, 16):
        raise ValueError("Unsupported binary log schema")
    if zlib.crc32(data[:60]) != struct.unpack_from("<I", data, 60)[0]:
        raise ValueError("Header CRC mismatch")
    header = dict(zip(HEADER_FIELDS, struct.unpack_from("<11I", data, 16)))
    if header["payload_bytes"] > 24 * 1024 or header["records"] > header["payload_bytes"] // 9:
        raise ValueError("Invalid payload size or record count")
    return header

def read_varint(data: bytes, offset: int, end: int) -> tuple[int, int]:
    value = 0
    for index in range(5):
        if offset >= end:
            raise ValueError("Truncated varint")
        byte = data[offset]
        offset += 1
        if index == 4 and byte & 0xF0:
            raise ValueError("Varint exceeds uint32")
        value |= (byte & 127) << (index * 7)
        if not byte & 128:
            if not value or (index and not byte):
                raise ValueError("Noncanonical sparse varint")
            return value, offset
    raise ValueError("Unterminated varint")

def map_records(path: Path) -> tuple[list[dict], int]:
    records = json.loads(path.read_text(encoding="utf-8"))["functions"]
    packed = bytearray()
    for record in records:
        label = record["label"].encode("ascii")
        if len(label) > 23:
            raise ValueError("Invalid function label in PC map")
        packed.extend(struct.pack("<II24s", record["begin"], record["end"], label))
    fingerprint = 2166136261
    for byte in packed:
        fingerprint = ((fingerprint ^ byte) * 16777619) & 0xFFFFFFFF
    return records, fingerprint

def matching_map(fingerprint: int, directory: Path) -> Path | None:
    """Resolve older and newer firmware captures by exact map contents."""
    if not fingerprint:
        return None
    for path in sorted(directory.glob("pc-profile-map-v*.json"), reverse=True):
        if map_records(path)[1] == fingerprint:
            return path
    return None

def decode(data: bytes, map_path: Path | None = None) -> tuple[dict, list[dict], list[dict]]:
    header = read_header(data[:64])
    payload_end = 64 + header["payload_bytes"]
    if len(data) != payload_end + 8 or data[payload_end:] != END:
        raise ValueError("Truncated log, extra bytes or missing trailer")
    functions = None
    if map_path is not None:
        functions, fingerprint = map_records(map_path)
        if fingerprint != header["map_fingerprint"]:
            raise ValueError("PC function map does not match this firmware log")
    frames, profiles = [], []
    offset = 64
    while offset < payload_end:
        if offset + 9 > payload_end:
            raise ValueError("Truncated record envelope")
        length = struct.unpack_from("<H", data, offset)[0]
        end = offset + length
        if not 9 <= length <= 169 or end > payload_end:
            raise ValueError("Invalid record length")
        expected_crc = struct.unpack_from("<H", data, end - 2)[0]
        if binascii.crc_hqx(data[offset:end - 2], 0xFFFF) != expected_crc:
            raise ValueError("Record CRC mismatch")
        kind = data[offset + 2]
        fields = FRAME_FIELDS if kind == 1 else PROFILE_FIELDS if kind == 2 else None
        if fields is None:
            raise ValueError("Unknown record type")
        mask = struct.unpack_from("<I", data, offset + 3)[0]
        if mask >> len(fields):
            raise ValueError("Invalid sparse field mask")
        row = dict.fromkeys(fields, 0)
        position = offset + 7
        for index, field in enumerate(fields):
            if mask & (1 << index):
                row[field], position = read_varint(data, position, end - 2)
        if position != end - 2:
            raise ValueError("Unexpected trailing bytes in record")
        if kind == 1:
            expected = row["prep_us"] + row["bg_us"] + row["obj_us"] + row["mix_us"] + row["rgb_us"] + row["other_us"]
            row["partition_valid"] = int(expected == row["worker_us"] == row["draw_us"] and
                row["qfull_us"] + row["barrier_us"] == row["wait_us"] and row["dirty_jobs"] <= row["jobs"])
            row["device_valid"] = int(bool(row["flags"] & 4))
            row["fps"] = 1_000_000 / row["wall_us"] if row["wall_us"] else 0
            row["guest_pc_hex"] = f'{row["guest_pc"]:08X}'
            for key in ("wall", "run", "draw", "copy", "wait", "qfull", "barrier", "prep", "bg", "obj", "mix", "rgb", "other"):
                row[key + "_ms"] = row[key + "_us"] / 1000
            row["vbskip_percent"] = 100 * row["vblank_cycles"] / row["guest_cycles"] if row["guest_cycles"] else 0
            reads = row["ifetch_hits"] + row["ifetch_fills"]
            row["ifetch_hit_percent"] = 100 * row["ifetch_hits"] / reads if reads else 0
            row["pad_on"] = int(bool(row["flags"] & 1))
            row["bios_sram"] = int(bool(row["flags"] & 2))
            row["blocks_on"] = int(bool(row["flags"] & 32))
            row["alu_burst_on"] = int(bool(row["flags"] & 64))
            row["leaf_fetch_on"] = int(bool(row["flags"] & 128))
            frames.append(row)
        else:
            for rank in range(1, 4):
                index = row[f"id{rank}"]
                if index == 0xFFFF:
                    name = "none"
                elif functions is None:
                    name = f"id:{index}"
                elif index < len(functions):
                    name = functions[index]["name"]
                else:
                    raise ValueError("PC function ID outside matching map")
                row[f"function{rank}"] = name
            row["includes_export_pause"] = int(bool(row["flags"] & 2))
            profiles.append(row)
        offset = end
    if len(frames) + len(profiles) != header["records"]:
        raise ValueError("Record count mismatch")
    return header, frames, profiles

def summarize(header: dict, frames: list[dict], profiles: list[dict]) -> dict:
    summary = dict(header, retained_frames=len(frames), retained_profiles=len(profiles),
        invalid_frame_partitions=sum(not (f["partition_valid"] and f["device_valid"]) for f in frames),
        note="Diagnostic build; USB/logging overhead is not isolated. Run, Draw and Wait overlap.")
    if frames:
        wall = sum(f["wall_us"] for f in frames)
        summary.update(first_frame=frames[0]["frame"], last_frame=frames[-1]["frame"],
            retained_wall_seconds=wall / 1_000_000,
            weighted_fps=len(frames) * 1_000_000 / wall if wall else 0,
            median_frame_ms=statistics.median(f["wall_ms"] for f in frames),
            worst_frame_ms=max(f["wall_ms"] for f in frames),
            frame_number_gaps=sum(b["frame"] != ((a["frame"] + 1) & 0xFFFFFFFF) for a, b in zip(frames, frames[1:])))
        summary["mean_ms_per_frame"] = {key: sum(f[key + "_ms"] for f in frames) / len(frames)
            for key in ("run", "draw", "copy", "wait", "qfull", "barrier", "prep", "bg", "obj", "mix", "rgb", "other")}
    return summary

def download(port: str, timeout: float) -> bytes:
    try:
        import serial
    except ImportError as error:
        raise RuntimeError("USB capture requires pyserial: python -m pip install pyserial") from error
    with serial.Serial(port, 115200, timeout=0.2, write_timeout=1) as device:
        device.reset_input_buffer()
        device.write(b"cancel\ndump\n")
        deadline = time.monotonic() + timeout
        def exact(count: int) -> bytes:
            result = bytearray()
            while len(result) < count:
                if time.monotonic() >= deadline:
                    raise TimeoutError("USB download timed out; wait for the game to run and close other serial applications")
                result.extend(device.read(count - len(result)))
            return bytes(result)
        window = bytearray()
        while bytes(window) != MAGIC:
            window.extend(exact(1))
            if len(window) > 8:
                del window[0]
        header_bytes = MAGIC + exact(56)
        header = read_header(header_bytes)
        data = header_bytes + exact(header["payload_bytes"] + 8)
        decode(data)  # Check all records before saving a claimed complete download.
        return data

def write_csv(path: Path, rows: list[dict], base_fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]) if rows else base_fields)
        writer.writeheader()
        writer.writerows(rows)

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="RP2350 USB COM port; not the ESP32 controller port")
    source.add_argument("--input", type=Path, help="Decode an existing .bin file without hardware")
    source.add_argument("--list", action="store_true", help="List available USB/serial ports")
    parser.add_argument("--output", type=Path, help="Capture filename prefix; local timestamp is always appended. Bare names use the logs folder")
    parser.add_argument("--log-dir", type=Path, default=DEFAULT_LOG_DIR, help="Capture folder (default: GBAPrototype/logs)")
    parser.add_argument("--open", action="store_true", help="Open the generated charts in the default browser")
    parser.add_argument("--map", type=Path, help="Matching pc-profile-map-vNN.json (otherwise discovered by fingerprint)")
    parser.add_argument("--timeout", type=float, default=45)
    args = parser.parse_args()
    if args.list:
        from serial.tools import list_ports
        for port in list_ports.comports():
            print(f"{port.device}: {port.description} [{port.hwid}]")
        return
    if args.input:
        path = args.input
        data = path.read_bytes()
        captured = datetime.fromtimestamp(path.stat().st_mtime).astimezone()
        if args.output:
            path = save_capture(data, args.output, args.log_dir, captured)
    else:
        captured = datetime.now().astimezone()
        data = download(args.port, args.timeout)
        path = save_capture(data, args.output, args.log_dir, captured)
    map_path = args.map
    if map_path is None:
        map_path = matching_map(read_header(data[:64])["map_fingerprint"],
            Path(__file__).resolve().parents[1] / "output")
        if map_path is None:
            map_path = matching_map(read_header(data[:64])["map_fingerprint"],
                Path(__file__).resolve().parents[2] / "docs" / "evidence")
    header, frames, profiles = decode(data, map_path)
    stem = path.with_suffix("")
    write_csv(stem.with_name(stem.name + "-frames.csv"), frames, FRAME_FIELDS)
    write_csv(stem.with_name(stem.name + "-profiles.csv"), profiles, PROFILE_FIELDS)
    summary = summarize(header, frames, profiles)
    summary["capture_file"] = path.name
    summary["capture_local_time"] = captured.isoformat()
    stem.with_name(stem.name + "-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    from debug_charts import write_report
    chart = write_report(stem.with_name(stem.name + ".html"), header, frames, profiles, summary)
    print(f"Saved/decoded {len(data)} bytes: {len(frames)} frames, {len(profiles)} CPU profiles; {header['dropped_records']} older records overwritten.")
    if frames:
        print(f"Retained {summary['retained_wall_seconds']:.1f}s; weighted {summary['weighted_fps']:.2f} FPS; worst frame {summary['worst_frame_ms']:.1f} ms.")
    print(f"CSV and summary: {stem}-frames.csv, {stem}-profiles.csv, {stem}-summary.json")
    print(f"Charts: {chart}")
    if args.open:
        import webbrowser
        webbrowser.open(chart.resolve().as_uri())

if __name__ == "__main__":
    main()
