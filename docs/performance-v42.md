# v42: a compositor change driven by board logs

V42 replaces the general per-pixel layer search with window spans, sorted
background-row descriptors and packed layer values. It keeps the V41 USB
binary logger and the same Sonic ROM, BIOS, 240 MHz CPU and 30 MHz PSRAM.
The firmware is `output/GBAFeather-Sonic-v42-USB.uf2`; V41 is the unchanged
manual fallback. RP2350 performance after this change is not yet measured.

## What the physical logs show

Ten V41 captures from `2026-10-19_23-04-02-536` through
`2026-10-19_23-29-54-329` contain 3,343 frame records and periodic core-0
profiles. Some retained windows overlap; these are not 3,343 independent
benchmark frames. All decoded frame partitions are valid, and each retained
window has consecutive frame numbers. Results are in
`output/log-analysis-v42.json`. Different captures include different scenes
and input, so their FPS values are not an A/B experiment.

| Capture local time | Weighted FPS | Run ms/f | Draw ms/f | Mix ms/f | Wait ms/f |
| --- | ---: | ---: | ---: | ---: | ---: |
| 23:04:02 | 12.57 | 75.3 | 38.4 | 20.4 | 25.5 |
| 23:09:56 | 9.70 | 97.9 | 42.2 | 24.5 | 25.8 |
| 23:16:14 | 9.54 | 99.5 | 42.2 | 24.5 | 25.8 |
| 23:16:53 | 10.48 | 90.5 | 25.5 | 15.4 | 14.6 |
| 23:17:13 | 13.05 | 72.4 | 10.5 | 7.7 | 6.1 |
| 23:21:03 | 7.25 | 131.1 | 36.3 | 21.9 | 18.0 |
| 23:23:06 | 7.17 | 132.7 | 38.3 | 23.4 | 18.6 |
| 23:25:41 | 8.27 | 114.9 | 42.7 | 24.1 | 23.3 |
| 23:29:02 | 8.22 | 115.7 | 42.6 | 23.4 | 22.7 |
| 23:29:54 | 7.11 | 133.8 | 44.6 | 26.4 | 21.9 |

The two additional captures confirm the same bottlenecks. The latest has
nearly 160 dirty scanlines per frame, so little of its output can be reused
as unchanged rows. Mix is 59% of Draw. Its 112 ms/frame of Run excluding
renderer wait is still much larger than the render wait. IFetch hits are
95%, but about 1,225 instruction-window fills occur per frame; hit rate alone
does not mean instruction fetching is cheap. Dispatch and code fetch each
occupy about 15–19% in its individual CPU windows. There is just one ROM
block miss across all 309 retained frames, with 0.29 ms/s of decompression.
The HDMI-reset counter stays at 3 and cached free heap stays at 20,024 bytes
through both new windows. Neither an ongoing reset storm nor declining
reported heap explains this slow period. The periodic heap reading does not
measure transient allocations or fragmentation.

**The compositor is the largest measured renderer cost:** 53–73% of Draw.
Queue-full waits dominate synchronization; frame-end/memory barriers are
under 0.6 ms/frame on average in these captures. This argues against simply
making the queue deeper or removing more memory-write barriers. Mix covers
window selection, visible-layer selection, color effects and the final
RGB555 row write; RGB565 conversion is measured separately.

**Core 0 also remains a substantial bottleneck.** In the 7.17 FPS window,
Run minus render wait is 114.1 ms/frame. This includes HDMI interruption and
other elapsed work, so it is not a pure interpreter measurement. Its CPU
profiles repeatedly put dispatch and bus instruction fetching in the top
three. Submit samples include inlined waiting. Only top-three function IDs
are retained per profile; those percentages do not describe every function
or sum to a complete CPU distribution.

HDMI IRQ handling is about 148–149 ms per wall second throughout the logs,
roughly 15% of one core's elapsed time. Removing that cost would not provide
the several-fold improvement still needed. ROM decoding ranges from 0 to
5.5 ms per wall second across these captures and is zero in the 7.17 FPS
window. Copy is effectively zero. All retained frames used the existing
BIOS flash view, with no BIOS SRAM copy. Audio synthesis is already muted.

Run includes Wait and overlaps Draw on the other core. Do not add these
three timings. Eliminating every rendering wait in the 7.17 FPS window
would still leave much more than the 16.7 ms budget for 60 FPS. This renderer
change cannot by itself meet that target.

## Implementation

The previous compositor checked each enabled BG's window bit and register
priority for every pixel, constructed layer objects, and always searched
for two visible backgrounds even when only one could affect the result.

V42 resolves rectangular window membership at window boundaries. OBJ
window coverage adds run boundaries only outside the rectangular windows.
For each resulting span it builds a short, priority-sorted list of enabled
BG row pointers and precomputed tags. Packed 32-bit layer values hold the
color, layer ID and priority. A separate kernel skips second-layer searches
when neither normal alpha blending nor a visible semitransparent OBJ can
need them. Brightness effects and forced OBJ alpha retain their prior
ordering, clipping and target-mask behavior.

No new frame buffer, heap allocation, PSRAM reservation or queue job field
is introduced. The compositor kernels are checked in the final ARM ELF for
SRAM placement. Shared-video-memory write synchronization remains intact.

The PC decoder now discovers a saved `pc-profile-map-vNN.json` by exact
fingerprint, allowing V41 and V42 captures to resolve function names using
their respective maps. The binary record schema is unchanged.

## Controlled native comparison

`output/compositor-v42-host-benchmark.json` records 18 runs: three workloads,
three repeats, baseline/candidate order alternated. Both executables use
the current identical test harness, sources and MSVC /O2 flags; only the
compositor source differs. The renderer runs synchronously to measure its
work without host thread-scheduling noise. Workloads are scripted Sonic
frames; they do not reproduce a physical capture's exact input history.

| Warmup frames | Measured frames | Median Draw reduction | Median total time reduction |
| ---: | ---: | ---: | ---: |
| 0 | 1,800 | 20.8% | 8.8% |
| 3,600 | 1,200 | 22.1% | 8.8% |
| 12,000 | 600 | 21.2% | 6.4% |

CPU/memory hash, guest cycles, final PC and video hash match across all
paired workloads. These are **x86 host timings, not RP2350 FPS predictions**.
An earlier frozen probe executable was unsuitable as the controlled
baseline; its result is excluded from these comparisons.

## Validation and next board measurement

The independent v21 renderer comparison covers 480 fixtures, including 240
directed window-span/effect cases: wrapped, empty and overlapping windows,
OBJ windows, effects, all modes, crop/full output and RGB565 margins. The
concurrent register-snapshot comparison covers 38,400 scanlines. The
6,000-frame Sonic regression compares CPU/banked state, bus/cycles, RAM and
all pixels against the synchronous original-ROM path, including target
replacement and binary log export. Its 20 checkpoints match V41.

Flash V42, visit the same slow scene and take a new capture:

```powershell
python <workspace>/GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v42.bin --open
```

Compare weighted FPS, Run, Mix, Draw and Qfull on comparable scenes. Check
the core-0 profiles after the rendering wait decreases: dispatch/fetch may
then account for a larger fraction without becoming slower. Keep the V41
image for a matched fallback capture. Neither native tests nor the prior
physical logs establish V42 startup or an on-board speedup.

Modified core, embedded ROM, BIOS and firmware remain private local files.
