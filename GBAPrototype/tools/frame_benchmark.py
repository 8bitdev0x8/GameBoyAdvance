"""Capture frozen fixed-input/fixed-frame benchmarks without timing-reset queries.

The MCU owns the interval; host poll speed and USB export cannot move endpoints.
No flashing or resets here. Root/perf-lab orchestrates those with a board lock.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import time

import benchmark_native
import debug_log
import perf_lab

VERSION = "0.1.2"
FIELDS = ("phase", "frame", "warmup", "count", "policy", "start_state", "end_state",
          "start_video", "end_video", "start_cycle", "end_cycle")


def parse_aot_status(line):
    values = line.strip().split()
    if len(values) != 11 or values[0] != "AOT" or values[1] not in ("0", "1"):
        raise ValueError("Malformed Flash AOT status")
    entries = int(values[2], 10)
    counters = [int(value, 16) for value in values[3:]]
    if entries not in (128, 512, 4096) or any(value < 0 or value > 0xffffffff for value in counters):
        raise ValueError("Flash AOT status outside supported range")
    return dict(ready=values[1] == "1", entries=entries, **dict(zip(
        ("lookups", "positive_hits", "negative_hits", "catalog_hits", "misses", "probes", "collisions", "binding_failures"), counters)))


def validate_aot_execution(native, catalog, expected_entries=None):
    if expected_entries is not None and catalog["entries"] != expected_entries:
        raise ValueError("Flash AOT catalog size differs from the requested experiment")
    if not native["enabled"] or not catalog["ready"]:
        raise ValueError("Flash AOT trial did not retain a bound, enabled catalog")
    if native["compiled"] or native["mismatches"] or catalog["binding_failures"]:
        raise ValueError("Flash AOT trial compiled dynamically or lost code/ROM identity")
    if not native["batches"] or not native["instructions"] or not catalog["catalog_hits"]:
        raise ValueError("Flash AOT trial executed no translated catalog blocks")


def read_aot_execution(device, timeout=15):
    # Query only after the frozen capture. These commands cannot perturb its
    # timings, and distinguish executing translated code from a mode flag alone.
    native = benchmark_native.query_status(device, timeout=timeout)
    deadline = time.monotonic() + timeout
    next_query = 0
    while time.monotonic() < deadline:
        if time.monotonic() >= next_query:
            device.write(b"aot status\n")
            next_query = time.monotonic() + 1
        line = device.readline().decode("ascii", errors="replace")
        if line.startswith("AOT "):
            return dict(native=native, catalog=parse_aot_status(line))
    raise TimeoutError("No Flash AOT catalog execution status")


def parse_status(line):
    if line.startswith("BOOTFAIL "):
        raise RuntimeError("MCU boot failure: " + line.strip())
    values = line.strip().split()
    if values == ["BENCH", "0"]:
        raise ValueError("Firmware has no fixed-frame benchmark enabled")
    if len(values) != len(FIELDS) + 1 or values[0] != "BENCH":
        raise ValueError("Malformed benchmark status")
    row = {key: int(value, 10 if index < 5 else 16) for index, (key, value) in enumerate(zip(FIELDS, values[1:]))}
    if any(value < 0 or value > 0xffffffff for value in row.values()):
        raise ValueError("Benchmark status outside u32 range")
    if row["phase"] not in (1, 2, 3) or not 1 <= row["count"] <= 180 or row["policy"] not in (0, 1, 2, 3):
        raise ValueError("Unsupported benchmark configuration")
    if row["phase"] == 3 and row["frame"] != row["warmup"] + row["count"]:
        raise ValueError("Frozen benchmark stopped at the wrong guest frame")
    return row


def keys_for_frame(frame, policy):
    if policy == 0:
        return 0
    if policy == 2:
        return 1 if 300 <= frame < 305 else 0
    if policy == 3:
        if any(start <= frame < start+5 for start in (300, 600, 1000)): return 1
        if any(start <= frame < start+5 for start in (800, 1200)): return 8
        return 0
    value = 0
    if 300 <= frame < 305: value = 1
    if 800 <= frame < 805: value = 1 << 3
    if frame >= 1200 and frame % 300 < 8: value |= 1
    if 2400 <= frame < 2408: value = 1 << 3
    if frame >= 3600: value |= 1 << 4
    return value


def validate_window(status, header, frames):
    if status["phase"] != 3:
        raise ValueError("Benchmark has not frozen its endpoints")
    if header["dropped_records"]:
        raise ValueError("Benchmark ring overwrote records")
    expected = list(range(status["warmup"] + 1, status["warmup"] + status["count"] + 1))
    if [f["frame"] for f in frames] != expected:
        raise ValueError("Capture is not the exact requested guest-frame range")
    if any(f["keys"] != keys_for_frame(f["frame"] - 1, status["policy"]) for f in frames):
        raise ValueError("Recorded input differs from the fixed replay policy")
    elapsed_cycles = (status["end_cycle"] - status["start_cycle"]) & 0xffffffff
    if sum(f["guest_cycles"] for f in frames) != elapsed_cycles:
        raise ValueError("Frame cycle deltas do not match frozen CPU endpoints")


def compare(a, b):
    # Match work before calculating a performance difference. Physical heap,
    # map and IRQ elapsed time can differ; guest endpoints and inputs cannot.
    for field in ("warmup", "count", "policy", "start_state", "end_state", "start_video", "end_video", "start_cycle", "end_cycle"):
        if a["benchmark"][field] != b["benchmark"][field]:
            raise ValueError("Benchmark workload/state differs: " + field)
    for field in ("cpu_hz", "psram_hz"):
        if a["metrics"][field] != b["metrics"][field]:
            raise ValueError("Benchmark clocks differ: " + field)
    afps, bfps = a["metrics"]["weighted_fps"], b["metrics"]["weighted_fps"]
    return dict(baseline_fps=afps, candidate_fps=bfps, fps_change=bfps-afps,
                fps_change_percent=(bfps/afps-1)*100,
                baseline_run_outside_wait_ms=a["metrics"]["run_outside_wait_ms"],
                candidate_run_outside_wait_ms=b["metrics"]["run_outside_wait_ms"],
                qualification="Fixed frame/input workload with equal CPU/RAM/video endpoint hashes; repeat trials quantify timing noise")


def capture(serial_number, stem, profile_map, timeout=1200, poll=2, engine="interpreter", aot_entries=128):
    import serial
    deadline = time.monotonic() + timeout
    device, last_status, target = None, None, None
    last_report = 0
    try:
        while time.monotonic() < deadline:
            if device is None:
                try:
                    target = perf_lab.resolve_target(serial_number)
                    device = serial.Serial(target.port, 115200, timeout=.2, write_timeout=1)
                    device.reset_input_buffer()
                except (OSError, RuntimeError):
                    if device is not None: device.close()
                    device = None
                    time.sleep(.5)
                    continue
            try:
                device.write(b"bench status\n")
                response_deadline = min(deadline, time.monotonic() + poll)
                while time.monotonic() < response_deadline:
                    line = device.readline().decode("ascii", errors="replace")
                    if not line.startswith(("BENCH ","BOOTFAIL ")):
                        continue
                    last_status = parse_status(line)
                    if time.monotonic() - last_report >= 20 or last_status["phase"] == 3:
                        print("Frame benchmark:", last_status, flush=True)
                        last_report = time.monotonic()
                    if last_status["phase"] == 3:
                        data = benchmark_native.read_dump(device, timeout=45)
                        header, frames, profiles = debug_log.decode(data, profile_map)
                        validate_window(last_status, header, frames)
                        expected_map = debug_log.map_records(profile_map)[1]
                        metrics = perf_lab.save_report(data, stem, profile_map, expected_map,engine)
                        if metrics["health_issues"]:
                            raise ValueError("Unhealthy fixed-frame capture: " + "; ".join(metrics["health_issues"]))
                        execution = None
                        if engine == "flash-aot":
                            execution = read_aot_execution(device)
                            perf_lab.write_json(stem.with_name(stem.name + "-aot-execution.json"), execution)
                            validate_aot_execution(**execution, expected_entries=aot_entries)
                        result = dict(tool_version=VERSION, benchmark=last_status, metrics=metrics, serial=target.serial, port=target.port,
                                      captured_local=perf_lab.local_time(), map=str(profile_map))
                        if execution is not None: result["aot_execution"] = execution
                        perf_lab.write_json(stem.with_name(stem.name + "-benchmark.json"), result)
                        return result
                # Limit traffic even when a warmup response arrives immediately.
                time.sleep(.1)
            except OSError:
                device.close();device=None
        raise TimeoutError(f"Fixed-frame benchmark did not finish; last status={last_status}")
    finally:
        if device is not None: device.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial")
    parser.add_argument("--map", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=float, default=1200)
    parser.add_argument("--engine", choices=("interpreter","flash-aot"), default="interpreter")
    parser.add_argument("--aot-entries", type=int, choices=(128,512,4096), default=128)
    parser.add_argument("--compare", nargs=2, type=Path)
    args = parser.parse_args()
    if args.compare:
        print(json.dumps(compare(*(json.loads(p.read_text()) for p in args.compare)), indent=2))
        return
    if not args.serial or not args.map or not args.output:
        parser.error("Capture requires --serial, --map and --output")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    print(json.dumps(capture(args.serial, args.output, args.map, args.timeout,engine=args.engine,aot_entries=args.aot_entries), indent=2))


if __name__ == "__main__":
    main()
