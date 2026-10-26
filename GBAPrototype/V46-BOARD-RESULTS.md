# V46 board captures: CPU work, queue waits and transition stutters

Seven captures from 20 October 2026 validate against V46's exact native map,
fingerprint `1213048002`. Every retained frame passes the timing-partition and
device-validity checks. Fetch is on throughout; blocks and ALU burst are off.
The user reports **about one FPS gained**. A controlled baseline/candidate pair
on the same gameplay scene is not present, so the logs do not quantify that gain.

**The user identified the latest capture as a menu or transition, not the slow
gameplay workload.** The other captures have no confirmed scene labels. Their
different averages must not be presented as gameplay speedups or as achieving
the 60 FPS target.

## Retained windows

| Capture, Dublin local time | Frames | Seconds | Weighted FPS | Run | Draw | Wait |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 02:24:00 | 125 | 15.43 | 8.10 | 118.01 | 19.37 | 12.85 |
| 02:24:26 | 333 | 24.66 | 13.50 | 70.17 | 22.66 | 14.04 |
| 02:25:07 | 294 | 26.25 | 11.20 | 84.36 | 26.38 | 12.55 |
| 02:25:32 | 361 | 20.14 | 17.93 | 52.48 | 8.18 | 4.32 |
| 02:26:17 | 363 | 16.51 | 21.99 | 42.59 | 10.66 | 6.79 |
| 02:26:26 | 402 | 17.16 | 23.43 | 39.91 | 5.42 | 3.33 |
| 02:27:28, menu/transition | 296 | 26.51 | 11.17 | 85.08 | 28.10 | 17.38 |

Run, Draw and Wait are ms/frame. Run includes Wait and interruption on core 0.
Draw runs on core 1 and overlaps Run. **Do not add these columns.** FPS is frame
count divided by retained elapsed time, not the average of instantaneous FPS.

There are 2,174 frame records, including 184 exact duplicates from overlapping
exports: **1,990 unique frames over 139.320 seconds**. Their weighted average is
14.28 FPS across retained, mixed scenes. This is not a whole-session average,
gameplay benchmark or controlled comparison with V45. Gaps are unobserved.

## The full native CPU profiles

After duplicate removal and exclusion of export-contaminated or incomplete
histograms, 25 windows retain **63,423 samples** over 130.858 seconds. Mapped
hits total 62,293; the remaining 1,130 samples are unmapped, with zero invalid
exception frames. Each window's mapped + unmapped + invalid counts equals its
sample count. Histogram and retained-frame intervals differ; do not assume
their aggregate denominators describe exactly the same time span.

| Sampled native function or group | Share of all eligible samples |
| --- | ---: |
| `Arm7tdmi::cpu_run_until`, main function | 24.57% |
| `ScanlineWorker::submit_hook` | 13.67% |
| `Bus::read` | 10.59% |
| `Arm7tdmi::execute_arm` | 7.77% |
| Fixed-width ARM fetch | 3.68% |
| `Arm7tdmi::execute_thumb` | 3.51% |
| General instruction fetch | 2.59% |
| Fixed-width Thumb fetch | 2.46% |
| DMA service | 1.81% |
| `Bus::write` | 1.53% |
| Main hardware service | 1.45% |
| Cartridge ROM read | 1.42% |

These are sampled native locations, not guest opcode frequencies. In particular,
the CPU loop includes **inlined fetch and Thumb ALU work**, so its entire 24.57%
must not be called avoidable dispatch overhead. Renderer submission includes
inlined queue waiting. Generic bus reads cover several address regions and
cannot be attributed entirely to PSRAM without address-region measurements.
The hottest listed execution/fetch/bus functions already reside in SRAM in
the exact V46 ELF; simply marking them time-critical again does not help.

The sampler has lower priority than HDMI. Eligible windows contain zero samples
of an interrupted IRQ handler, despite the separately timed HDMI cost below.
Its function shares are biased toward the main-thread locations where sampling
can be delivered. **Do not add HDMI's timed percentage to these sampled shares.**

The sampling ISR itself totals 114,578 microseconds over 130.858 seconds,
approximately **0.088%**. This excludes histogram serialization, logging,
diagnostic drawing and USB export; their combined overhead remains unisolated.

## Latest menu/transition window: two different problems

The latest capture averages **89.55 ms/frame**, Run 85.08, Draw 28.10 and Wait
17.38 ms/frame. Run outside explicit renderer wait is **67.70 ms/frame**. This
includes guest execution, memory access, scheduling and interrupt preemption;
it is not pure ALU time. The latest window's native profile also puts the CPU
loop first, followed by renderer submission and generic bus reads.

Queue-full waiting is 17.16 ms/frame; the separate video-memory barrier is only
0.22 ms/frame. Increasing queue depth alone cannot remove sustained worker
throughput limits. BG and composition account for 9.77 and 11.26 ms/frame of
Draw; sprites add 3.41, setup 1.68, RGB output 1.77 and other work 0.21 ms/frame.

Holding all other work fixed and subtracting all measured Wait leaves
72.17 ms/frame, roughly **13.9 FPS**. This is an arithmetic bound for that
single intervention on this window, not a prediction of a rewritten renderer.
Both CPU execution and rendering need further reductions to meet 16.7 ms/frame.
It is not a measured budget for the user's seven-FPS gameplay scene.

The separately timed HDMI handler costs 147.04 ms/s, about **14.70%** of elapsed
time in this capture. ROM decompression costs 15.76 ms/s, about **1.58%** on
average. It is therefore not the dominant sustained cost here, but the worst
transition frames show a significant separate problem:

| Frame | Total ms | ROM misses | Decompression ms |
| --- | ---: | ---: | ---: |
| 2944 | 156.91 | 4 | 54.97 |
| 3061 | 139.13 | 2 | 32.58 |
| 2965 | 137.91 | 1 | 19.44 |
| 3049 | 137.13 | 1 | 19.71 |

There are 25 misses across 20 of the 296 retained frames. Decompression therefore
matters to **transition stutter** even though its average cost is small. ROM
cache/decode work and sustained interpreter performance need separate tests.
The latest instruction-window hit rate is 96.02%; a hit still performs guest
timing, prefetch, latch and instruction-execution work.

## Stability and memory

All seven captures report a constant **8,528 bytes free heap**, 1,112 fewer
than V45's observed 9,640 bytes, matching the static SRAM increase. The HDMI
reset counter stays at three in every retained frame. Its earlier resets are
not observed, so these logs do not establish their cause or absence before
recording. Constant heap does not prove adequate peak stack or heap headroom.

The dependent-access benchmark reports S32 21 ns, P32 797 ns and P256 1209 ns.
These are dependent 32-bit reads across **32 KiB SRAM, 32 KiB PSRAM and 256 KiB
PSRAM working sets**, respectively. P256 is not a 256-byte transfer. The smaller
PSRAM test is roughly 38 times the SRAM test, including live IRQ, loop and cache
effects; this is not raw chip latency or a measurement of total PSRAM stall time.

## Next experiment

1. Establish the seven-FPS gameplay workload. The latest supplied file is
   explicitly a menu/transition; the other scenes are unclassified. Preserve
   the user-reported one-FPS gain as an observation, not a measured A/B result.
2. On one repeatable gameplay scene, use the existing runtime switch to compare
   fetch on/off/on. All supplied V46 frames have fetch on, so they cannot show
   whether the inline leaf helps on the RP2350. No new firmware is needed for
   this comparison. Return to the same scene and allow equal warmup each time.
3. Split the CPU-loop bucket into source-level fetch, ALU and bookkeeping
   locations, and classify guest code/data regions before choosing a larger
   execution or RAM-access change. Existing function-level samples cannot
   separate those inlined operations or quantify PSRAM stalls.
4. Keep BG/composition and queue throughput as the renderer targets. Preserve
   the confirmed timing and pixel regressions; do not skip guest events to
   inflate the counter. Profile ROM page churn separately for transition pauses.

Example commands from `<workspace>`, using the Feather's USB port:

```powershell
python GBAPrototype/tools/block_control.py --port COM9 --engine fetch --mode on
# Stay on the repeatable gameplay scene for at least 20 seconds.
python GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v46-gameplay-fetch-on --open
python GBAPrototype/tools/block_control.py --port COM9 --engine fetch --mode off
# Return to that scene and repeat the same warmup.
python GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v46-gameplay-fetch-off --open
python GBAPrototype/tools/block_control.py --port COM9 --engine fetch --mode on
```

The last command restores the default. USB export pauses gameplay. Captures
should retain multiple complete uncontaminated five-second CPU windows after
each mode change. A 60 Hz HDMI signal is not 60 FPS emulation. The benchmark
remains **smooth 60 FPS gameplay**, which these logs do not establish.

No firmware or core behavior was changed in this analysis. V46 and its V45
fallback remain byte-for-byte intact. Numerical evidence and input SHA256s are
in `output/log-analysis-v46.json` and `output/log-analysis-v46-overview.json`;
`build/analyze_v46.py` at the workspace root reproduces their checks and
aggregation. Raw captures and supplied game assets remain local.
