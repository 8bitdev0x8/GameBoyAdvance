# V51: specialize the ordinary interpreter slice

V50's latest capture is a **demo-play scene**, as identified by the user.
The interpreter is working and native translation remains off, but throughput
is still about 7.46 FPS. V51 tests a bounded CPU-loop improvement: select
optional engines once per slice and compile their disabled paths out of the
ordinary interpreter loop. It does not introduce another translator.

## V50 board findings

All four new captures match the V50 function map, have native/blocks/burst off,
fast fetch/memory enabled and PAD ON throughout. Their timing partitions and
complete histograms validate. The captures represent different intervals;
only the last has an independently confirmed scene label.

| Capture, Dublin local time on 20 October 2026 | Frames | Retained seconds | Weighted FPS |
| --- | ---: | ---: | ---: |
| 19:53:07, unclassified | 306 | 26.44 | 11.57 |
| 19:54:51, unclassified | 396 | 16.97 | 23.34 |
| 19:56:16, unclassified | 341 | 26.11 | 13.06 |
| **19:57:06, demo play** | **267** | **35.78** | **7.46** |

The last capture averages Wall 134.03, Run 127.28, Draw 36.48 and Wait
17.45 ms/frame. Run outside explicit renderer waits is **109.83 ms/frame**.
BG and composition account for 16.97 and 14.17 ms/frame on core 1.
Copy is effectively zero. These overlapping times must not be added.

HDMI remains healthy at 31,500 IRQ/s and 147.2 ms/s in its handler. Its reset
counter stays three; cached free heap stays 8,336 B. ROM decoding averages
7.62 ms/s (about 0.76%), with 23 misses across the retained frames. It can
contribute to individual stalls, but does not explain most sustained frame time.

Seven complete uncontaminated profile windows retain 17,246 samples, with
zero invalid samples and 1,174 unmapped samples (6.81%). The main buckets are:

| Exclusive sampled core-0 location | Share |
| --- | ---: |
| CPU loop, including inlined fetch/ALU/bookkeeping | 30.55% |
| Scanline submission, including queue waiting | 12.85% |
| General bus read | 7.48% |
| ARM execution | 6.88% |
| Thumb execution | 4.54% |
| Fixed-width Thumb / ARM fetch | 2.60% / 2.53% |

The high-priority HDMI handler is timed separately and is not added to these
conditional thread sample shares. Full decoded results are retained in
`output/log-analysis-v50.json`. These scene-dependent windows do not show a
controlled gain over V49 or establish 23 FPS in the slow demo workload.

## Change

Previously `cpu_run_until()` contained both the ordinary interpreter and the
optional native, predecoded-block and ALU-burst paths. Runtime flags disabled
the latter, but their checks and bodies remained in the hot function.

V51 dispatches at the start of each CPU slice:

- `run_interpreter_slice()` uses a compile-time specialization without those
  optional branches. The active loop remains in internal SRAM.
- `run_experimental_slice()` retains the old optional-engine behavior in
  cached flash. Native/block/burst opt-in is still available.

The template shares one source implementation of the guest timing logic.
The 16-instruction hardware-service cadence, per-instruction target checks,
prefetch bookkeeping, halt/IRQ checks, DMA checks and instruction semantics
remain in place. Engine settings are selected between calls, as the firmware's
USB command handling already does at frame boundaries.

This reduces normal-loop work and SRAM occupancy. It also changes the optional
experimental loop's placement to flash; an experiment using that loop must
account for the new placement. A V51 native-on/off trial is therefore not a
measurement of exactly the old V50 native path. Native still defaults off.
No extra reclaimed memory is allocated to another cache in this build.

ROM, BIOS, 240 MHz clock, PSRAM timing, UART, renderer and HSTX recovery remain
the same. V50 is retained as the complete firmware fallback.

## Local measurements

The host comparison uses preserved V50 CPU code versus the candidate, with
the same ordinary interpreter settings. It warms 3,600 scripted Sonic frames
then measures 600 frames, alternating order across three trials per mode.
CPU/memory/cycle and frame checksums match.

| Host workload | V50 median | Candidate median | Elapsed change |
| --- | ---: | ---: | ---: |
| CPU with rendering skipped | 282.678 ms | 271.754 ms | **−3.86%** |
| Threaded renderer | 391.857 ms | 375.474 ms | **−4.18%** |

These are MSVC desktop timings, not Feather FPS. They cover the scripted
workload, not a replay of the captured physical demo scene.

A separate comparison compiles the real CPU/bus code for Cortex-M33 and
executes it in Unicorn, counting host instructions with native translation
off in both versions:

| Synthetic guest loop | V50 host instructions | Candidate | Change |
| --- | ---: | ---: | ---: |
| Register ALU | 11,917,781 | 11,382,114 | **−4.49%** |
| Mixed ALU/RAM | 11,972,241 | 11,475,063 | **−4.15%** |

Guest registers, flags and cycles match. These counts exclude physical
PSRAM/XIP timing, HDMI interruption and bus contention. As the earlier JIT
trial showed, these measurements justify a board trial, not an assumed FPS gain.

Linked static SRAM falls from **246,980 to 245,484 B**, saving **1,496 B**.
Actual runtime free heap still needs board confirmation. Individual compiled
stack frames are 8 B for the dispatcher, 80 B for the interpreter slice and
96 B for the experimental slice; these are not whole-stack peak measurements.

Packaging verifies 262,412 B between the linked heap start and stack reserve,
up 1,488 B after linker alignment. This is preallocation space, not runtime
free heap. The exact final map has 1,061 ranges, fingerprint 1,113,170,417.

## Correctness and reproducibility

- The 6,000-frame compressed-ROM/threaded RGB565 test preserves full CPU,
  timing, bus/prefetch/latch, RAM and pixel parity with the conservative path.
  All twenty retained V50 checkpoints match. It includes display-target
  replacement at frame 3,000.
- The existing core audit passes, including independent arithmetic checks,
  instruction fetch/data cases, DMA/timers, block and burst execution.
- The 4,130 emitted-native fixtures and 115,276 slices pass through the new
  optional-engine dispatch path, including native mode transitions. This is
  fixture coverage, not another 1,200-frame actual-native game replay.
- Binary decoder and benchmark-tool regression tests pass. The final two-pass
  firmware map is checked against the exact final ELF before packaging.

For a local baseline comparison, prepare the private preserved source before
building the two `gba_v51_*` targets:

```powershell
python GBAPrototype/tests/prepare.py --baseline-archive GBAPrototype/output/source-sonic-v50-private.zip
cmake --build build/gba-host --config Release --target gba_v51_baseline gba_v51_candidate
```

The baseline target is optional when its generated reference source is absent.
The current core and generated reference remain private under the existing
publication boundary. This does not make the public support/docs checkout a
standalone complete emulator build.

Retained evidence includes `v51-host-benchmark.json`, `v51-arm-work.json`,
`sonic-v51-default.log`, `core-v51-audit.log` and `native-v51-audit.json` under
the local output directory. Layout, hash, map and memory checks are recorded
in `output/performance-v51-summary.json`.

## Board trial

Flash the local `output/GBAFeather-Sonic-v51-USB.uf2`, confirm **v51 / J0**, and
capture the same slow demo-play portion used for the last V50 log. The V51
UF2 has not been flashed by the agent. Its board FPS and stability are unmeasured.

```powershell
python <workspace>/GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v51-demo --open
```

Compare matched scenes and render work, not a menu against demo play. The
existing native A/B helper compares native versus interpreter within one build;
it does not compare V50 versus V51. Keep V50 if this candidate does not produce
a repeatable board benefit. This is a modest optimization trial; the much larger
CPU-work reduction needed for 60 FPS remains unresolved.
