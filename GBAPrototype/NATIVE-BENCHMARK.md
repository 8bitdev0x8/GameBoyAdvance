# Next board experiment: interpreter/native/interpreter

V49/V50 support this experiment. Before changing the small native-code cache,
measure the current translator against its interpreter on the same board.
The local helper `tools/benchmark_native.py` uses commands already present
in V49; it does not rebuild or flash the device.

## Run

1. Keep V49 running. Enter a Sonic gameplay area where the character can stand
   still safely. Keep the camera/location steady and leave the controls untouched.
2. Close Serial Monitor and other programs holding COM9.
3. Run:

```powershell
python <workspace>/GBAPrototype/tools/benchmark_native.py --port COM9 --seconds 30
```

The default sequence takes about 105 seconds plus serial-command/export time:

| Phase | Engine | Warmup | Measurement |
| --- | --- | --- | --- |
| A1 | Interpreter | 5 s | 30 s |
| B | Native translation | 5 s | 30 s |
| A2 | Interpreter | 5 s | 30 s |

The helper clears the volatile ring at the start of each measurement. Export
any earlier logs you want to keep before running it. It retains all completed
phase captures on the PC and attempts to restore the original native, optional
block and ALU-burst settings after completion, errors or Ctrl+C. If the device
disconnects or stops responding, restoration cannot be guaranteed; the manifest
records that error and the tool prints a warning.

## Output

Each run creates a new folder under `GBAPrototype/logs`, for example:

```text
logs/native-ab-2026-10-20_20-15-00-123456/
    comparison.json
    a1-interpreter.bin
    a1-interpreter-frames.csv
    a1-interpreter-profiles.csv
    a1-interpreter-summary.json
    b-native.bin
    ...
    a2-interpreter.bin
    ...
```

The example timestamp is illustrative. Actual names use the PC's local time.
An existing trial folder is never overwritten. `comparison.json` includes
each phase's weighted FPS, median/p95/worst frame time, Run/Draw/Wait, HDMI
cost and resets, retained duration, input state, clocks/map, free heap, and
native status-counter deltas. Its native phase reports executed guest
instructions per native call and compilations per native call. V49 does not
provide explicit eviction or cache-hit counters.

Raw captures use the existing CRC-checked binary protocol. Matching PC maps
are discovered from the local output directory. Native status snapshots
bracket the host measurement period; the following dump starts at another
frame boundary, and its ring may retain only a subset. Counter intervals,
frame intervals and CPU-profile intervals are therefore reported separately.

To generate the usual browser charts for an individual phase:

```powershell
python <workspace>/GBAPrototype/tools/debug_log.py --input <workspace>/GBAPrototype/logs/native-ab-EXACT-RUN-FOLDER/b-native.bin --open
```

Replace `EXACT-RUN-FOLDER` with the folder printed by the benchmark.

## Interpret the result

The final comparison combines A1/A2 by total frames divided by total retained
time. It does not average their FPS arithmetically. It reports native FPS
relative to that baseline, plus A1/A2 drift. Warnings flag:

- More than 10% drift between the two interpreter observations.
- Changing inputs, partial/mixed controller disconnection or optional engine contamination.
- Abnormal video interrupt cadence or changing HDMI reset counts.
- Firmware/clock changes or substantial ring truncation.

A clean trial is still a **sequential steady-scene observation**, not a save-state
replay. Enemy phases, camera movement and other scene changes can affect it.
Repeat it before adopting a default mode. For strict comparisons, a later
input/state replay facility is preferable.

Uniform PAD OFF with unchanged zero keys is recorded as a CPU-throughput note,
not a mixed-connection warning. It does not validate input responsiveness.
The first completed trial and its slower native result are documented in
[V50 findings](PERFORMANCE-v50.md); its original manifest is preserved even
though the warning classification has since been refined.

The next code experiment depends on the result. If native mode loses, use the
interpreter as the performance baseline and inspect compile/reuse overhead
before expanding translation. If native mode wins consistently, test improved
retention/admission as a separate candidate against both V49 modes. Neither
outcome by itself establishes the remaining roughly 5.7x throughput needed
for the latest gameplay window to reach 60 FPS.

## Local validation

Nine simulated-device tests cover the full sequence and binary partial reads,
mode restoration, optional-engine restoration, Ctrl+C, wrong-mode rejection,
counter wrap, weighted comparison, drift/retention/input/video warnings and
protection against overwriting an existing trial folder. These validate the
host tool, not RP2350 performance. The user subsequently completed the first
physical A/B/A trial; it measured a native-mode regression.
