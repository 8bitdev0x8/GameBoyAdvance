"""V49 native/interpreter A/B/A board trial, using existing USB commands.

Keep Sonic in the same area without touching the controls. This is a steady-
scene trial, not a deterministic save-state replay. No firmware is flashed.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import statistics
import time

import block_control
import debug_log

COUNTERS = ("lookups", "compiled", "batches", "instructions", "memory", "exits", "mismatches")


def query_status(device, prefix=b"", timeout=15, clock=time.monotonic, engine="native"):
    label, parser = {"native": ("JIT ", block_control.parse_native_status),
                     "blocks": ("BLK ", block_control.parse_status),
                     "burst": ("BST ", block_control.parse_burst_status)}[engine]
    command = f"{engine} status\n".encode("ascii")
    device.write(prefix + command)
    deadline = clock() + timeout
    next_query = clock() + 1
    while clock() < deadline:
        line = device.readline().decode("ascii", errors="replace")
        if line.startswith(label):
            return parser(line)
        if clock() >= next_query:
            device.write(command)
            next_query = clock() + 1
    raise TimeoutError("No native status: use V49, close other serial programs, and check the Feather port")


def set_mode(device, enabled, **kwargs):
    status = query_status(device, b"native on\n" if enabled else b"native off\n", **kwargs)
    if status["enabled"] != enabled:
        raise RuntimeError("Firmware did not apply the requested native mode")
    return status


def read_dump(device, timeout=45, clock=time.monotonic):
    device.write(b"cancel\ndump\n")
    deadline = clock() + timeout

    def exact(count):
        result = bytearray()
        while len(result) < count:
            if clock() >= deadline:
                raise TimeoutError("Binary export timed out")
            result.extend(device.read(count - len(result)))
        return bytes(result)

    window = bytearray()
    while bytes(window) != debug_log.MAGIC:
        window.extend(exact(1))
        if len(window) > 8:
            del window[0]
    header_bytes = debug_log.MAGIC + exact(56)
    header = debug_log.read_header(header_bytes)
    data = header_bytes + exact(header["payload_bytes"] + 8)
    debug_log.decode(data)
    return data


def interval_stats(before, after):
    return {key: (after[key] - before[key]) & 0xffffffff for key in COUNTERS}


def phase_metrics(header, frames, profiles, enabled):
    if not frames:
        raise ValueError("Capture contains no completed frames")
    summary = debug_log.summarize(header, frames, profiles)
    if summary["invalid_frame_partitions"] or summary["frame_number_gaps"]:
        raise ValueError("Invalid timing or discontinuous frame records")
    if any(bool(frame["native_on"]) != enabled for frame in frames):
        raise ValueError("Capture contains the wrong native mode or mixed modes")
    if any(frame["blocks_on"] or frame["alu_burst_on"] for frame in frames):
        raise ValueError("Optional block/burst engine contaminated the interpreter baseline")
    wall = sum(frame["wall_us"] for frame in frames)
    sorted_ms = sorted(frame["wall_ms"] for frame in frames)
    return dict(
        frames=len(frames), retained_seconds=wall / 1e6,
        weighted_fps=len(frames) * 1e6 / wall,
        median_frame_ms=statistics.median(sorted_ms),
        p95_frame_ms=sorted_ms[max(0, (95 * len(sorted_ms) + 99) // 100 - 1)],
        worst_frame_ms=max(sorted_ms),
        mean_ms=summary["mean_ms_per_frame"],
        irq_per_second=sum(frame["hdmi_irqs"] for frame in frames) * 1e6 / wall,
        irq_ms_per_second=sum(frame["hdmi_irq_us"] for frame in frames) * 1e3 / wall,
        decode_ms_per_second=sum(frame["decode_us"] for frame in frames) * 1e3 / wall,
        resets=sorted({frame["hdmi_resets"] for frame in frames}),
        keys=sorted({frame["keys"] for frame in frames}),
        pad_on_frames=sum(frame["pad_on"] for frame in frames),
        minimum_heap=min(frame["free_heap_cached"] for frame in frames),
        map_fingerprint=header["map_fingerprint"],
        cpu_hz=header["cpu_hz"], psram_hz=header["psram_hz"],
        dropped_records=header["dropped_records"],
    )


def compare(phases, requested_seconds):
    first, native, last = phases
    baseline_frames = first["frames"] + last["frames"]
    baseline_seconds = first["retained_seconds"] + last["retained_seconds"]
    baseline_fps = baseline_frames / baseline_seconds
    drift = abs(last["weighted_fps"] - first["weighted_fps"]) / (
        (last["weighted_fps"] + first["weighted_fps"]) / 2) * 100
    warnings = []
    notes = []
    if drift > 10:
        warnings.append("Interpreter A1/A2 differ by more than 10%; scene drift makes the comparison inconclusive")
    if len({p["map_fingerprint"] for p in phases}) != 1 or len({(p["cpu_hz"], p["psram_hz"]) for p in phases}) != 1:
        warnings.append("Firmware/clock identity changed between phases")
    if any(p["retained_seconds"] < requested_seconds * .8 for p in phases):
        warnings.append("Ring overwrote a substantial part of a measurement; results describe retained windows only")
    if any(not 25000 <= p["irq_per_second"] <= 38000 for p in phases):
        warnings.append("Abnormal HDMI interrupt rate")
    if any(len(p["resets"]) != 1 for p in phases) or len({v for p in phases for v in p["resets"]}) != 1:
        warnings.append("HDMI restart counter changed during the trial")
    if len({tuple(p["keys"]) for p in phases}) != 1 or any(len(p["keys"]) != 1 for p in phases):
        warnings.append("Controller input changed; this is not an unchanged-input trial")
    if all(p["pad_on_frames"] == 0 and p["keys"] == [0] for p in phases):
        notes.append("Controller disconnected throughout with unchanged zero input; this measures CPU throughput, not controller response")
    elif any(p["pad_on_frames"] != p["frames"] for p in phases):
        warnings.append("Controller connection was absent for some frames")
    return dict(
        interpreter_weighted_fps=baseline_fps,
        native_weighted_fps=native["weighted_fps"],
        observed_native_fps_change_percent=(native["weighted_fps"] / baseline_fps - 1) * 100,
        interpreter_a1_a2_drift_percent=drift,
        warnings=warnings,
        notes=notes,
        interpretation="Inconclusive trial; inspect warnings" if warnings else
            "Steady-scene observation; repeat with matching input/state before accepting a general speedup",
    )


def wait_seconds(seconds, clock=time.monotonic, sleep=time.sleep):
    deadline = clock() + seconds
    while clock() < deadline:
        sleep(min(.25, deadline - clock()))


def run_trial(device, directory, seconds=30, warmup=5, timeout=45,
              clock=time.monotonic, sleep=time.sleep):
    """Device is exclusively held by caller. Restore its initial mode on exit."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    manifest = dict(started_local=datetime.now().astimezone().isoformat(),
                    seconds_requested_per_phase=seconds, warmup_seconds=warmup,
                    phases=[], original_mode_restored=False,
                    scope="Sequential interpreter/native/interpreter steady-scene trial, not a deterministic replay")
    original = None
    try:
        device.reset_input_buffer()
        device.write(b"cancel\n")
        original = query_status(device, timeout=timeout, clock=clock)
        manifest["original_native_enabled"] = original["enabled"]
        original_blocks = query_status(device, timeout=timeout, clock=clock, engine="blocks")["enabled"]
        original_burst = query_status(device, timeout=timeout, clock=clock, engine="burst")["enabled"]
        manifest.update(original_blocks_enabled=original_blocks, original_burst_enabled=original_burst)
        query_status(device, b"blocks off\nburst off\n", timeout=timeout, clock=clock)
        for name, enabled in (("a1-interpreter", False), ("b-native", True), ("a2-interpreter", False)):
            print(f"{name}: {warmup:g}s warmup, then {seconds:g}s measurement. Keep controls untouched.", flush=True)
            set_mode(device, enabled, timeout=timeout, clock=clock)
            wait_seconds(warmup, clock, sleep)
            # clear removes old ring records. Status acknowledges that this
            # command reached a frame boundary, then resets timing baselines.
            before = query_status(device, b"clear\n", timeout=timeout, clock=clock)
            started = clock()
            wait_seconds(seconds, clock, sleep)
            after = query_status(device, timeout=timeout, clock=clock)
            elapsed = clock() - started
            data = read_dump(device, timeout, clock)
            path = directory / (name + ".bin")
            path.write_bytes(data)
            raw_header = debug_log.read_header(data[:64])
            map_path = debug_log.matching_map(raw_header["map_fingerprint"], Path(__file__).resolve().parents[1]/"output")
            header, frames, profiles = debug_log.decode(data, map_path)
            metrics = phase_metrics(header, frames, profiles, enabled)
            counters = interval_stats(before, after)
            metrics.update(name=name, native_enabled=enabled, capture_file=path.name,
                           host_counter_window_seconds=elapsed, native_counters=counters,
                           instructions_per_native_call=counters["instructions"] / counters["batches"] if counters["batches"] else None,
                           compiled_per_native_call=counters["compiled"] / counters["batches"] if counters["batches"] else None,
                           exact_map=map_path.name if map_path else None)
            # Native status counters bracket the host measurement. The dump
            # follows at another frame boundary; their intervals are adjacent,
            # not claimed identical to the retained frame/profile intervals.
            metrics["counter_interval_note"] = "Status-counter interval is separate from retained frame/profile intervals"
            manifest["phases"].append(metrics)
            debug_log.write_csv(directory/(name+"-frames.csv"), frames, debug_log.FRAME_FIELDS)
            debug_log.write_csv(directory/(name+"-profiles.csv"), profiles, debug_log.PROFILE_FIELDS)
            summary = debug_log.summarize(header, frames, profiles)
            (directory/(name+"-summary.json")).write_text(json.dumps(summary, indent=2)+"\n")
            print(f"  {metrics['weighted_fps']:.2f} FPS; retained {metrics['retained_seconds']:.1f}s; p95 {metrics['p95_frame_ms']:.1f} ms", flush=True)
        manifest["comparison"] = compare(manifest["phases"], seconds)
        manifest["completed"] = True
    except BaseException as error:
        manifest["completed"] = False
        manifest["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if original is not None:
            try:
                # Cancel a partial dump before asking the firmware to switch.
                device.write(b"cancel\n")
                device.reset_input_buffer()
                set_mode(device, original["enabled"], timeout=timeout, clock=clock)
                for engine, key in (("blocks", "original_blocks_enabled"), ("burst", "original_burst_enabled")):
                    if key in manifest:
                        expected = manifest[key]
                        status = query_status(device, f"{engine} {'on' if expected else 'off'}\n".encode("ascii"),
                                              timeout=timeout, clock=clock, engine=engine)
                        if status["enabled"] != expected:
                            raise RuntimeError(f"Could not restore {engine} setting")
                manifest["original_mode_restored"] = True
            except BaseException as error:
                manifest["restore_error"] = f"{type(error).__name__}: {error}"
                print("Could not confirm original mode restoration. Check native status before playing.", flush=True)
        (directory/"comparison.json").write_text(json.dumps(manifest, indent=2)+"\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--warmup", type=float, default=5)
    parser.add_argument("--timeout", type=float, default=45)
    parser.add_argument("--log-dir", type=Path, default=debug_log.DEFAULT_LOG_DIR)
    args = parser.parse_args()
    if not 10 <= args.seconds <= 120 or not 0 <= args.warmup <= 60 or not 1 <= args.timeout <= 120:
        parser.error("Use 10–120 measurement seconds, 0–60 warmup seconds and 1–120 timeout seconds")
    stamp = datetime.now().astimezone().strftime("%Y-%m-%d_%H-%M-%S-%f")
    directory = args.log_dir / ("native-ab-" + stamp)
    import serial
    with serial.Serial(args.port, 115200, timeout=.2, write_timeout=1) as device:
        manifest = run_trial(device, directory, args.seconds, args.warmup, args.timeout)
    comparison = manifest["comparison"]
    print(f"Interpreter {comparison['interpreter_weighted_fps']:.2f} FPS; native {comparison['native_weighted_fps']:.2f} FPS")
    print(f"Observed difference {comparison['observed_native_fps_change_percent']:+.1f}%; A1/A2 drift {comparison['interpreter_a1_a2_drift_percent']:.1f}%")
    for warning in comparison["warnings"]:
        print("WARNING:", warning)
    for note in comparison["notes"]:
        print("NOTE:", note)
    print(comparison["interpretation"])
    print("Results:", directory.resolve()/"comparison.json")


if __name__ == "__main__":
    main()
