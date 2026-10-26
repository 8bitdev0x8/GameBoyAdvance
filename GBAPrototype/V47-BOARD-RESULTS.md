# V47: confirmed gameplay remains CPU limited

The user identifies the last of four new captures,
`sonic-v47-gameplay-2026-10-20_17-06-06-790.bin`, as gameplay. The earlier
three captures have no confirmed scene labels. All four match map fingerprint
`3026315552`, pass the frame timing checks, and have memory/fetch on with
blocks and ALU burst off. No same-scene V46 comparison or memory-off trial
is supplied.

| Capture, Dublin local time | Frames | Retained seconds | FPS | Run | Draw | Wait |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 17:03:39, unclassified | 313 | 22.12 | 14.15 | 66.49 | 26.97 | 13.20 |
| 17:04:00, unclassified | 304 | 24.99 | 12.16 | 77.52 | 26.63 | 13.01 |
| 17:05:02, unclassified | 294 | 26.11 | 11.26 | 84.05 | 34.58 | 23.19 |
| 17:06:06, **gameplay** | 272 | 35.84 | **7.59** | **125.02** | **35.43** | **14.16** |

Run/Draw/Wait are ms/frame and overlap. Across the four mixed windows there
are 1,183 frame records, 64 exact duplicates and 1,119 unique frames over
104.546 seconds. Their 10.70 FPS weighted average is not gameplay performance.

## The gameplay budget

The confirmed gameplay window averages **131.78 ms/frame**. Its median is
129.81 ms, 95th percentile 152.40 ms and worst frame 164.05 ms. The real-time
target of about 16.7 ms needs roughly **7.9 times current throughput**.

| Measurement | Gameplay result |
| --- | ---: |
| Run, including renderer waits and core-0 interruption | 125.02 ms/frame |
| Explicit renderer waiting, already inside Run | 14.16 ms/frame |
| Run outside that waiting | **110.85 ms/frame** |
| Queue-full / video-write barrier | 13.81 / 0.36 ms/frame |
| Draw on core 1 | 35.43 ms/frame |
| BG / composition | 17.04 / 13.62 ms/frame |
| OBJ / preparation / RGB / other | 1.48 / 1.01 / 1.96 / 0.32 ms/frame |
| Dirty scanlines | 160/frame |
| HDMI handler, separately timed | 147.12 ms/s, about 14.71% elapsed |
| ROM decompression | 3.52 ms/s, about **0.35%** elapsed |
| ROM instruction-window hit rate | 95.86% |
| Observed free heap | 8,240 bytes throughout |
| HDMI reset counter | Two throughout |

Removing all measured Wait while holding other costs fixed leaves roughly
117.61 ms/frame, **8.50 FPS**, not 60. Draw itself also needs about a 2.1-fold
reduction to fit a 16.7 ms budget. BG and composition are 86.5% of Draw, making
them the renderer targets; queue depth alone cannot fix worker throughput.

There are 11 ROM misses across 11 of 272 frames. They sometimes add 7–18 ms
and contribute to stutter, but completely removing average decompression alone
would change 7.59 FPS to only about 7.62 FPS under a fixed-other-work model.
This is not the dominant sustained gameplay bottleneck.

## Complete native CPU profiles

Seven complete uncontaminated gameplay profile windows retain 17,165 samples
over 35.426 seconds, slightly different from the retained frame interval.
Invalid frames are zero; 1,103 samples (6.43%) are unmapped. Every complete
histogram's mapped + unmapped + invalid counts equals its sample count.

| Native location | Share of eligible samples |
| --- | ---: |
| Main CPU loop | **32.97%** |
| Renderer submission, including inlined queue waiting | 10.82% |
| General bus reads | 7.31% |
| ARM execution | 6.58% |
| Thumb execution | 5.05% |
| Fixed Thumb / fixed ARM / general instruction fetch | 2.72 / 2.58 / 1.47% |
| Cartridge reads | 1.87% |
| General bus writes | 1.48% |
| DMA service | 1.11% |
| Main hardware service / timer service / schedule refresh | 0.99 / 0.90 / 0.77% |

The loop bucket includes inlined cached fetch, ALU execution and bookkeeping.
It is **not 33% pure dispatch overhead**. Function-level samples cannot separate
these operations or identify which guest instructions dominate. End-of-frame
guest PCs are not an unbiased guest instruction profile. Unmapped native PCs
also need location information before attributing their cost.

Sampling is lower priority than HDMI: all seven windows record zero interrupted
IRQ contexts, despite HDMI's separately measured cost. Do not add the 14.71%
HDMI timer to these conditional native function shares. The sampling ISR itself
is about 0.103% elapsed over its windows; serialization/logging/HUD overhead is
not included in that figure or isolated here.

## CPU execution work to do next

1. Split the large native CPU-loop bucket into address/source regions and
   retain bounded information about unmapped native PCs. Add guest execution
   location and memory-region attribution to identify hot ROM, BIOS and RAM
   loops. Use bounded counters, not a per-instruction USB trace.
2. Prototype a **mixed-instruction execution block** for the measured hot
   workload, covering useful ALU/load/store/control-flow sequences rather
   than another ALU-only decoder. The earlier narrow block prototype and ALU
   burst are disabled because their host results did not justify enabling
   them. Decoding once is insufficient if every opcode still pays the full
   generic fetch, bus and timing path.
3. Specialize safe block transitions and common memory accesses, keeping
   guest registers/cycle work local where possible. Preserve architectural
   fetch cycles, open-bus/prefetch latches and event cadence. Exit or fall back
   on scheduled hardware events, MMIO side effects, DMA/IRQ, mode changes or
   mutable-code invalidation. This is proposed work, not an implemented backend.
4. Compare identical gameplay with the backend on/off and preserve exact
   CPU/cycle/bus/RAM/pixel regressions. Reuse the current disabled cache's SRAM
   budget where feasible; current free heap is only 8,240 bytes. Any larger
   native-code cache needs an explicit allocation budget. Do not assume that
   PSRAM is suitable for hot interpreter metadata.
5. If block interpretation still cannot remove enough overhead, evaluate a
   genuine hot-block native translator as a separate architecture experiment.
   Existing predecode is not JIT translation. Neither approach has established
   60 FPS on this board. In parallel, BG/composition need renderer optimization.

The immediate goal is a measurable reduction in the 110.85 ms CPU-side remainder,
with a detailed breakdown explaining it. A small isolated gain is not evidence
of a path to the roughly 7.9-fold total improvement needed for 60 FPS.

## Evidence and limits

`output/log-analysis-v47.json` retains each capture's digest, exact map,
statistics and full native distribution. `output/log-analysis-v47-overview.json`
deduplicates frame records and eligible CPU windows. The workspace script
`build/analyze_v47.py` reproduces CRC/map, timing, duplicate and sample-accounting
checks. Nineteen unique eligible windows across all scenes contain 46,522
samples; this mixed-scene distribution must not replace the gameplay table.

These observations confirm V47 boots and runs gameplay. Free heap is constant,
not proof of peak memory safety. The reset counter's earlier transitions are
unobserved. The current samples do not establish a controlled V46/V47 speedup,
PSRAM stall fraction or the benefit of the runtime memory switch. No firmware
is changed by this analysis; V47 and its V46 fallback remain intact.
