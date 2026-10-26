# V45 board results and the next CPU investigation

Seven binary captures on 20 October 2026 match V45's exact native symbol-map
fingerprint `1969481180`. All retained frames have valid timing partitions.
Both optional CPU block execution and the uncached ALU burst are off.

| Capture, Dublin local time | Frames | Retained seconds | Weighted FPS | Run | Draw | Wait |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 01:39:07 | 83 | 9.9 | 8.39 | 113.7 | 19.1 | 12.5 |
| 01:40:15 | 348 | 31.5 | 11.06 | 85.5 | 27.7 | 13.7 |
| 01:40:55 | 410 | 24.9 | 16.45 | 57.2 | 7.8 | 4.0 |
| 01:41:42 | 438 | 18.8 | 23.26 | 40.0 | 5.0 | 3.0 |
| 01:42:44 | 401 | 30.5 | 13.16 | 71.8 | 16.0 | 10.6 |
| 01:43:54 | 329 | 41.7 | 7.89 | 120.5 | 33.6 | 13.9 |
| 01:44:22 | 330 | 45.6 | **7.24** | **131.5** | **35.8** | **14.7** |

Times after FPS are ms/frame. The exports overlap: 2,339 retained records
contain 126 exact duplicates, leaving **2,213 unique frames over 185.672
seconds**, a weighted 11.92 FPS across those windows. This is neither the
whole session average nor a controlled comparison with V44. The user reports
gameplay around seven FPS; the two latest slow captures support that report.

## The slow-window budget

The 01:44:22 capture's average frame takes **138.17 ms**. Run contains renderer
waiting, and Draw overlaps Run on core 1; they must not be added.

| Measurement | Value |
| --- | ---: |
| Run | 131.48 ms/frame |
| Wait, already inside Run | 14.70 ms/frame |
| Run outside explicit renderer wait | **116.78 ms/frame** |
| Draw on core 1 | 35.76 ms/frame |
| Background / composition | 17.13 / 14.19 ms/frame |
| Preparation / objects / RGB / other | 1.01 / 1.15 / 1.96 / 0.30 ms/frame |
| Queue-full / explicit barrier | 14.37 / 0.34 ms/frame |
| HDMI handler elapsed cost | 147.09 ms/s, about 14.7% |
| ROM decompression | 3.43 ms/s, about 0.34% |
| Instruction-window hit rate | 95.70% |
| Dirty lines rendered | 160/frame |
| Observed free heap | 9,640 bytes |

About **89% of Run is outside explicit renderer waiting**. This remainder
includes interpreter dispatch, instruction/data accesses, device scheduling,
interrupt preemption and any unclassified work; it is not pure guest ALU time.
Removing only the observed Wait while holding other work fixed leaves about
123.5 ms/frame, roughly **8.1 FPS**. This is a limited arithmetic illustration,
not a prediction of a different renderer or memory system. It shows why a
render-only patch cannot close the approximately **8.3×** gap from this window
to the 16.7 ms/frame target.

Top-three-only native PC windows attribute about 21.9% to CPU dispatch, 12.3%
to fixed-width Thumb fetch and 10.1% to renderer submission, which includes
inlined queue waiting. They omit smaller functions; 6.2% of valid samples
are unmapped in this capture. These are sampled function locations, not an
exclusive complete CPU-time partition or guest-instruction mix.

The HDMI reset counter rises from six in the earliest retained capture to
seven in the next, then stays seven in all later retained frames. The transition
is outside those retained windows, so its exact timing and cause are unknown.
The observed heap remains constant, without proving sufficient peak stack space.

## Next steps and acceptance criterion

**The target is smooth 60 FPS gameplay. It is not yet achieved.**

1. Investigate CPU dispatch/fetch/bus and event-service overhead together,
   rather than attributing all Run time to renderer stalls.
2. Test an inline cached-opcode leaf against the V45 fetch path. A cache miss,
   peripheral, boundary or unusual prefetch state must keep the audited path;
   architectural fetch cycles, latches and DMA/IRQ ordering remain required.
3. Capture more than the three largest PC functions and identify the guest
   instruction/address/event workload on the slow gameplay scene. Desktop
   scripted entry/menu sequences are not substitutes for that scene.
4. Use controlled same-scene board A/B windows and runtime mode flags. A large
   gap may require a broader execution backend, not another small cache lookup
   or queue-depth adjustment. A proposed translator is not an implemented JIT.
5. Retain exact CPU/cycle/bus/RAM/pixel regressions and a working UF2 fallback.

The inline-fetch V46 source experiment is **work in progress**. Its synthetic
160,000-fetch audit passes, including 85,434 eligible cached hits, but that is
not proof of improved board speed or full firmware readiness. At this
publication checkpoint V45 remains the latest packaged and board-measured
build. No 60 FPS claim is made.

Numerical evidence is in `output/log-analysis-v45.json` and
`output/log-analysis-v45-overview.json`. Raw binary captures remain local.
