# Validation and current emulator state

## Hardware result

Latest author report, 19 October 2026, **v28 with Sonic Advance 3**:

| Metric | Result |
| --- | --- |
| Boot, display and controller | User confirmed the build works |
| Emulated FPS | Approximately 7 |
| Controller | PAD ON |
| VBskip | 12% |
| Run | 113 ms/frame |
| Draw | 22 ms/frame |
| Copy | 0 ms/frame |
| Wait | 14 ms/frame |
| Exact game scene / measurement duration | Not supplied |
| PSRAM benchmark table / HDMI handler ms/s | Not supplied |
| V28 HDMI resets, IRQ/s, ROM misses/decode | Not supplied |
| 30 FPS minimum / 60 FPS target | Not reached |

The retained hardware reports form a development record rather than a controlled benchmark:

| Version | FPS | VBskip | Run | Draw | Copy | Wait |
| --- | --- | --- | --- | --- | --- | --- |
| v24 | About 5 | 15-21% | Not supplied | 46 | 10 | Not supplied |
| Inferred v25 | About 6 | 12% | 144 | 37 | 10 | Not supplied |
| v26 | About 6-7 | 6-12% | Not supplied | 49 | 0 | 26 |
| v27 | About 7 average | 10% | 121 | 33 | 0 | 32 |
| v28 | About 7 | 12% | 113 | 22 | 0 | 14 |

All timing columns are ms/frame. The v25 association is inferred from delivery/report order because that report did not name the flashed version. Scenes and durations were unspecified. Draw 33 to 22 and Wait 32 to 14 are lower reported v28 readings, with FPS still about 7; they do not establish a same-scene speedup. Earlier v23 measured roughly 4-5 FPS. The v21 claim of 7 FPS was corrected by the user to a brief peak, with sustained speed below 5 FPS.

V27 additionally reported HDMI resets 5, IRQ/s 31495 and usually zero ROM misses/decode with occasional spikes. Reset count includes two planned startup resets and is not a reset frequency. V26 reported ROM misses 0 and decode 0-19 ms/s. None of those values should be silently carried forward as a v28 measurement.

Run is core 0 emulation/finish elapsed time and includes Wait; Draw is core 1 render/conversion/presentation elapsed time and can overlap Run. Interrupts contribute to the affected core's timings. Do not add these values as sequential exclusive CPU costs. Copy zero reflects the v26 removal of the full-frame presentation copy, with conversion/presentation now included in Draw. VBskip is the percentage of guest cycles bypassed in recognized VBlank loops, not hardware utilization.

The HSTX monitor signal is 60 Hz even when only about seven new guest frames complete per second. The latest result demonstrates a running experiment, not real-time gameplay. Full utilization of both cores and the PSRAM share of frame cost remain unmeasured. V28 contains the memory and IRQ timing instruments, but PC sampling, DWT/XIP/bus profiling, JIT and a new accelerated scanout backend are proposals rather than active features.

## Native regression checks through v28

The retained v28 run uses 6000 frames, the local replacement BIOS, scripted inputs, firmware-like 1024-cycle slices and a real host render thread. It compares compressed-ROM, fast-fetch, video-optimized, accelerated, silent-APU direct output with synchronous, original-ROM, ordinary-fetch, conservative-video, unaccelerated, synthesis-enabled execution.

At slice/frame boundaries it checks full CPU/banked state and flags, exact guest cycles, bus/prefetch/latch state, EWRAM/IWRAM and every RGB565 display pixel plus protected margins. Display storage is replaced at frame 3000. Twenty PC/hash/input-read checkpoints match v27 and the earlier preserved Sonic sequence. Final results:

| Native result | Value |
| --- | ---: |
| Emulated frames | 6000 |
| Final PC | `080bcca6` |
| Final framebuffer hash | `5ba30470` |
| Guest cycles | 1685292894 |
| KEYINPUT reads | 11861 |
| Renderer scanline jobs completed | 958723 |
| VBlank guest cycles bypassed | 347342144 (20.61%) |
| SRAM ROM cache hits | 117386291 |
| SRAM ROM cache misses | 187416 |
| Backing-page misses | 1048 |

The equivalent older unaccelerated sequence had 225930343 SRAM-ROM-cache hits. The reduction represents fewer callbacks from work that no longer executes, not fewer original ROM bytes or a measured hardware speedup. Backing misses were unchanged. The scripted sequence reaches language/name entry; it does not complete registration or full gameplay.

V25 preserved those execution checkpoints while moving tiny hot calls inline. V26 validated real-thread direct display output, protected margins, forced blank/clean rows and draining/detaching pending work. V27's audit reproduced old-code Thumb LDMIA/STMIA base-in-list failures before fixing them; 8160 nonempty-list cases now pass. Both corrected cases executed zero times in the scripted Sonic run, so neither is an established cause of its low FPS or prior game stalls. A further 30000 mixed fetch comparisons preserve data/cycles/bus state, and 60000 randomized timer operations match a separately compiled preserved v26 timer implementation. The fast fetch path covers 89126473 of 109063662 instruction reads, about 81.7%, rather than demonstrating an 81.7% speedup.

V28 adds 45003 video-write/read checks, 600 VRAM DMA fixtures and same-value barrier/dirty-state checks. They compare cycles, latches, effective memory, alignment/mirrors, ignored/replicated byte writes, DMA channel state and IRQs. The native Sonic work counters are:

| V28 native counter | Result |
| --- | ---: |
| Video writes | 4196721 |
| Unchanged video writes | 4141934 (98.69%) |
| Storage display-register half writes | 1154176 |
| Unchanged register halves | 1039108 (90.03%) |
| Conservative-reference decoded tile-row misses | 8011631 |
| Optimized decoded tile-row misses | 3848078 (51.97% fewer) |
| Small-range invalidations | 4732 |
| Worker jobs, v27 / v28 | 958816 / 958723 |

Most visible lines still render. These counts describe the scripted sequence, not board time, CPU utilization, an FPS multiplier or complete-game workloads.

Other retained checks include:

- Sixteen VBlank wait cases covering BEQ/BNE, prefetch off/on, variable event slices, HBlank/VBlank/timer IRQs, wake-up and rejection of other status masks.
- RAM-poll event slices, EWRAM/IWRAM, prefetch, flag wake-up and unsupported-MMIO rejection.
- Every one of 1024 compressed Sonic blocks, random/boundary reads and corrupt input rejection.
- SRAM cache line boundaries and two-way conflicts, associative backing-cache behavior and arena alignment/capacity/overflow handling.
- Save allocation reuse, EEPROM serial round trips and Flash64 identification/program/erase/load.
- Short press, hold/release, disconnect and frame-counter rollover.
- Silent APU register/status, FIFO-request and event behavior under randomized writes.
- Text map sizes, tile-cache generation wrap and independent video-memory invalidation.
- Video DMA idle and IRQ-progress timeouts, including timer wraparound.

The latest PPU comparison passes 240 randomized fixtures against the independently compiled v21 renderer across modes 0-5, windows, priority ties, transparency/blending, text layouts/flips, affine backgrounds and sprites. It now includes direct RGB565 output/protected margins, cropped framebuffer fallback, sparse tile-row invalidation and large-range fallback. The native paths share substantial implementation, so they can share bugs; these are regression checks within an experimental core, not conformance against a physical GBA. See the [v27 audit](core-audit-v27.md) and [v28 findings](performance-v28.md).

## Firmware image checks

The private v28 image compiled with Arduino-Pico 5.4.4, C++20 and `-O3`, separate translation units, 8 MiB PSRAM and no USB stack. CPU remains 240 MHz and PSRAM 30 MHz. Validation checked UF2 block magic/order/count, flash bounds, vector/stack addresses, SRAM hot-code and benchmark placement, exact packed ROM inclusion, PSRAM bounds and ELF initialized-RAM bytes against UF2 contents. CPU/timer/APU/worker, clock/linker and ROM-cache sources are preserved from v27; the installed HDMI driver is unchanged.

| Private v28 artifact statistic | Value |
| --- | ---: |
| Arduino-reported program size | 4942308 bytes |
| Static SRAM | 227108 bytes |
| Space from static end to heap limit before runtime allocations | 280788 bytes |
| Main stack reservation | 16384 bytes |
| Core 1 stack reservation | 8192 bytes plus 64-byte canary |
| Initialized RAM load bytes verified | 49572 bytes |
| Physical flash address span | 5004288 bytes |
| UF2 container length | 10009088 bytes |
| Fixed PSRAM arena usage | 7832576 of 7864320 bytes |

UF2 framing makes its file length larger than the physical flash span. These are numbers from a local image containing privately supplied game data, not a firmware download offered in this repository.

## Known limits

The emulator remains slow and experimental. There is no public full build because the upstream core lacks redistribution permission. Audio output and persistent saves are absent. Single-buffer presentation can tear. EEPROM timing and compatibility are incomplete; empty-register-list ARM/Thumb transfers and ARM block-transfer PC/base edge cases need further accuracy work. Complete game correctness has not been established. Earlier apparent game stalls, extra HDMI resets and two no-start optimization regressions were not fully root-caused.

The current publication does not rerun a private ROM test in a clean public checkout: the required core and game assets are intentionally absent. The text logs record the local tests that were run. Compilation of support sketches and repository publication checks are separate from those emulator tests.
