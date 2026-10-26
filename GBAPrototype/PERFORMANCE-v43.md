# v43: fixed-width CPU instruction fetching

V43 targets the CPU/bus cost identified in six V42 hardware captures. It
uses separate ARM-word and Thumb-halfword fetch routines, retains V42's
renderer and USB logging, and keeps the same ROM, BIOS and clock settings.
The candidate is `output/GBAFeather-Sonic-v43-USB.uf2`; the board-tested
V42 image remains the manual fallback. V43 board performance is unmeasured.

## Findings from the new board logs

The six captures run from `sonic-v42-2026-10-19_23-48-38-060.bin` through
`sonic-v42-2026-10-19_23-54-42-725.bin`. Their exact map fingerprint is
1088876913. They contain 2,113 retained frame records; overlapping windows
are not independent benchmark runs. Each retained window is consecutive,
and every decoded frame's timing partitions are valid. Full measurements
are in `output/log-analysis-v43.json`.

| Capture | Weighted FPS | Run ms/f | Draw ms/f | Mix ms/f | Wait ms/f |
| --- | ---: | ---: | ---: | ---: | ---: |
| 23:48:38 | 12.11 | 78.2 | 24.8 | 9.8 | 13.9 |
| 23:50:03 | 11.86 | 79.8 | 24.1 | 9.8 | 15.1 |
| 23:52:12 | 11.97 | 79.0 | 16.0 | 6.5 | 7.4 |
| 23:53:39 | 8.11 | 116.9 | 32.6 | 14.4 | 16.0 |
| 23:54:00 | 7.39 | 128.4 | 29.9 | 12.5 | 13.5 |
| 23:54:42 | 7.78 | 122.2 | 24.1 | 10.0 | 11.0 |

Mix is smaller in these windows than the 20–26 ms/f slow V41 windows.
Their inputs/scenes are not a controlled A/B replay, so this is not a
precise measurement of V42's speedup. The remaining slow windows have
about 101–115 ms/f of Run outside renderer waiting. This includes HDMI
preemption and other core-0 elapsed work; it is not pure CPU-instruction
time. CPU dispatch and code fetching lead the core-0 sample windows,
typically about 18–20% each in the slow scenes. Ordinary bus reads also
appear. Samples are exclusive host-PC observations, and only the top three
functions per window are retained.

Decompression is not the leading bottleneck: the six windows use
0–16.95 ms of decoding per wall second, at most about 1.7%. The latest
7.78 FPS window uses only 0.86 ms/s and three decoded-block misses in
45.1 seconds. Its instruction-window hit ratio is 94.76%, with about
1,192 instruction-window fills per frame. Those fills are different from
compressed-block misses. Even instruction-window hits still execute
guest bus timing, prefetch, lookup and latch bookkeeping.

HDMI IRQ handling remains around 148 ms per wall second, approximately
15% of one core's elapsed time. The latest window has no increase in
its HDMI-reset counter; periodically cached free heap stays at 20,424
bytes. This does not measure transient allocation peaks or fragmentation.
The data do not isolate PSRAM stalls from computation inside bus functions.

## Selected implementation

`Bus::read_code_specialized<Width>` supplies one out-of-line routine for
Thumb halfwords and another for ARM words. Constant width allows alignment,
load width, latch expansion and RAM guest-cycle cost to be compiled without
runtime width selection. Keeping the routines outside `cpu_run_until`
bounds the interpreter loop's hot code size. Both routines must reside in
internal SRAM in the final ARM image.

`Cartridge::read_instruction_fixed<Width>` first checks an already acquired
stable instruction line. A valid cached tag implies the provider and
header/GPIO guards were established when the line was acquired. Complete
bounds checks remain for a partial final ROM line. Misses use the existing
checked instruction reader; failed acquisition and provider replacement
invalidate the cached tag/pointer as before.

DMA or non-code accesses, EEPROM, BIOS, MMIO, unmapped addresses, unusual
ROM prefetch states and other nonmatching cases retain the generic reader.
The optimized path preserves returned values, guest cycles, bus latches,
prefetch transitions, ROM aliases and the existing boundary prefetch hints.
The cache and emulator class layouts do not need new storage.

The PC symbol map labels the two new functions `Fetch Thumb fixed` and
`Fetch ARM fixed`, so subsequent board samples can distinguish them from
the generic `Bus code fetch` fallback. A lower percentage in the old
generic function alone is not proof of improvement: sum the relevant fetch
functions and compare actual elapsed frame times.

## Experiments that were excluded

An initial fully inlined fetch path passed correctness tests but did not
show a dependable host improvement. Early timings also varied widely, so
the final experiment uses owned benchmark subprocesses pinned to the same
allowed logical CPU, high process priority, rotating variant order and
three repeats per workload.

A second experiment packed NZ/add/sub flag updates into a single CPSR
assignment. More than one million independent arithmetic fixtures passed,
but CPU-only host timings showed no consistent benefit. It was excluded
from the production candidate. `build/cpu-v43-experiments.zip` retains the
private experimental sources and benchmark harness; rejected timings are
kept for audit rather than counted as delivered gains.

## Controlled host results

`output/cpu-v43-host-benchmark.json` contains 36 runs: baseline, fetch-only,
flags-only and combined variants; three workloads; three repeats. Each
variant uses the same native MSVC /O2 build and forced fetch-wrapper
inlining. The baseline selects the V42 generic fetch and flag behavior.
The final production source corresponds to the **fetch-only** variant.
The exact guest cycles, PC, CPU/RAM hash and video hash match within each
workload across all variants and repeats.

| Workload | Warmup | Frames measured | Fetch-only median time reduction |
| --- | ---: | ---: | ---: |
| CPU, rendering disabled | 3,600 | 2,400 | 3.36% |
| CPU, rendering disabled | 12,000 | 1,200 | 3.46% |
| CPU plus synchronous rendering | 3,600 | 2,400 | 1.82% |

These are modest **x86 host** reductions, not predicted RP2350 FPS gains.
There is no evidence here for a several-fold improvement or 60 FPS.

## Validation and use

The fixed-width bus audit compares 30,000 mixed reads against the ordinary
bus, plus targeted RTC/GPIO, ROM boundaries and EEPROM cases. It checks
exact data, guest cycles and the full observable bus/prefetch/latch state.
The cartridge audit also compares both fixed widths at every byte offset
across 263,168 alignment/end-boundary fixtures, including a short ROM tail,
failed cache acquisition and replacing the provider.

The final 6,000-frame regression compares the compressed/threaded path with
the original-ROM/synchronous path: CPU/banked state, cycles, bus state,
working RAM, every RGB565 pixel and protected margins. It includes input,
display target replacement and binary log accounting. Twenty checkpoints
must match V42. Final UF2 checks cover vectors, stack layout, initialized
SRAM, ROM/BIOS bytes, function-map contents and SRAM placement.

Flash V43 and capture the same slow scene used for V42:

```powershell
python <workspace>/GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v43.bin --open
```

Compare weighted FPS, Run excluding Wait, total fetch samples, and Draw on
comparable scenes. Startup and performance must still be checked on the
board. The 60 FPS target requires a much larger reduction in CPU work and
further rendering improvements; lowering the physical RP2350 clock to the
guest GBA clock would reduce available computing capacity instead.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full system explanation.
Modified core, ROM, BIOS and firmware remain private local artifacts.
