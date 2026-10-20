# V44 board captures: 20 October 2026

Seven new USB captures confirm that V44 runs on the Feather and has a
near-20-FPS retained window. Performance remains strongly scene-dependent:
the longer captures range from 6.90 to 19.85 FPS. The logs do not establish
a uniform speedup against V42 because there is no identical input/scene
replay across the versions.

All seven headers identify exact V44 map fingerprint **173271835**, CPU
240 MHz and PSRAM 30 MHz. Every retained frame has CPU blocks **OFF**, so
the optional block interpreter cannot explain these results. These captures
include V43's fetch specialization and V44's compositor changes together.

## Retained measurements

Times below are from the capture filenames in local Europe/Dublin time.
The first two captures are too short for a sustained performance conclusion.

| Capture | Frames | Retained seconds | Weighted FPS | Run ms/f | Draw ms/f | Wait ms/f |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 00:50:28 | 1 | 0.1 | 9.78 | 79.0 | 2.7 | 0.0 |
| 00:51:01 | 54 | 6.1 | 8.91 | 106.8 | 16.5 | 9.9 |
| 00:51:25 | 333 | 29.5 | 11.30 | 84.1 | 21.0 | 13.7 |
| 00:52:00 | 347 | 31.3 | 11.07 | 85.4 | 26.4 | 13.2 |
| 00:53:25 | 393 | 19.8 | **19.85** | 47.1 | 10.1 | 5.9 |
| 00:54:58 | 332 | 32.3 | 10.29 | 92.3 | 33.1 | 19.4 |
| 00:56:42 | 330 | 47.9 | **6.90** | 138.1 | 35.2 | 14.0 |

The 1,790 retained records include 55 duplicate frames in overlapping early
captures. Exact duplicate checking leaves **1,735 unique frames**, spanning
160.74 seconds of observed frame time, at **10.79 weighted FPS**. Gaps
between the retained windows remain unobserved. This aggregate is not the
average of the whole play session and is not an average of the seven FPS
numbers. FPS uses frame count divided by summed frame wall time.

All CRC/format checks pass, every frame partition is valid, and each
individual window has consecutive frame numbers. The frame timing invariant
still holds: Draw phases sum to Draw, and Qfull + Barrier equals Wait.
Run includes Wait and IRQ preemption; Draw overlaps Run on the other core.

## HDMI and memory observations

The HDMI reset counter is **5 in every retained frame**. There is no newly
recorded reset during these windows. The first saved frame already has that
count; these logs do not record the earlier reset events or explain the
reported black screen. Successful recovery is reported by the user, not
deduced from the LED or reset count alone.

Reported free heap is consistently **11,088 bytes** across every retained
window. There is no observed downward trend, but cached heap samples cannot
rule out transient allocation peaks, fragmentation or earlier faults. V44
adds 4,528 static SRAM bytes and 3,352 emulator-object bytes versus V43; its
smaller remaining headroom is relevant when considering more caches or
independent video-memory snapshots.

## What still limits the slow scene

The latest capture retains 330 frames across 47.9 seconds. At 6.90 FPS,
mean wall time is about 144.8 ms/f:

- Run is **138.09 ms/f**, including **13.96 ms/f** of renderer waiting.
  About **124.13 ms/f** of Run remains outside that waiting. It still
  includes HDMI preemption and other work, so this is not pure instruction
  execution time.
- Draw is **35.23 ms/f**: background drawing **15.04**, compositing **14.26**,
  setup **2.51**, RGB/writeout **1.97**, sprites **1.15**, other **0.32**.
  Backgrounds plus mixing are roughly **83% of Draw**.
- Queue-full wait is **13.66 ms/f**; all barrier waits together are only
  **0.30 ms/f**. Removing shared-memory joins alone would not remove the
  large Run cost or the pixel work.
- HDMI handling remains about **147.3 ms per wall second**, roughly 14.7%
  of one core's elapsed time.
- Compressed ROM decoding takes **3.89 ms per wall second**, around 0.39%,
  with 16 decoded-block misses. Decompression is still a secondary cost.
- The instruction window hits **95.58%**, but fetch timing/cache/bus
  bookkeeping remains. In retained top-three CPU sample windows, dispatch
  contributes about **20.8%**, specialized Thumb fetch **12.8%**, ordinary
  bus reads **5.3%**, and scanline submission **4.1%**. These are partial
  sampled-PC observations, not a complete wall-time budget.

There are zero bad samples, but about 6.5% of valid-window samples in the
latest capture are unmapped. The exact symbol map matches; raw unknown PC
addresses would be needed to resolve those samples further. Current logs
retain their count, not the individual unknown addresses.

The best capture has lower CPU and rendering workloads: only about 92 dirty
scanlines per frame versus 160 in the latest slow window. Its 19.85 FPS is
real for that observed window, but does not demonstrate 20 FPS in the more
demanding scene.

## Next priorities supported by these captures

1. CPU dispatch, instruction-fetch and ordinary bus-read work remain the
   main critical-path targets. The narrow ALU predecode stays off after its
   host regression; a useful replacement needs to amortize more work per
   block and address fetch bookkeeping without breaking guest timing.
2. For drawing, target **background rendering and compositing together**.
   The latest scene spends similar time in each; optimizing only the mixer
   would leave nearly half of its renderer cost untouched.
3. Evaluate HDMI IRQ cost reduction/relocation with coordinated recovery,
   and measure its effect on renderer latency. Moving work to core 1 is an
   experiment, not a free elimination of its cost.
4. Queue-depth/per-scanline distributions and render-path counters would
   distinguish burst backpressure and choose useful BG/effect fast paths.
   More full-frame VRAM copies are not justified by 0.30 ms/f barrier time.

Use the same slow scene and input replay for a future before/after test.
No clock change, firmware modification or new claimed speedup was made as
part of analyzing these captures.

Machine-readable results:
[per-capture analysis](evidence/log-analysis-v44.json) and
[deduplicated overview](evidence/log-analysis-v44-overview.json).
Reproduce with `python <workspace>/build/analyze_v44.py`.
