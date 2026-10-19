# Validation and current emulator state

## Hardware result

Latest author report, 19 October 2026, **v24 with Sonic Advance 3**:

| Metric | Result |
| --- | --- |
| Boot, display and controller | User confirmed the build works |
| Emulated FPS | Approximately 5 |
| VBskip | 15-21% |
| Draw | About 46 ms/frame |
| Copy | About 10 ms/frame |
| Run timing / exact game scene | Not supplied |
| 30 FPS minimum / 60 FPS target | Not reached |

Earlier v23 measured roughly 4-5 FPS. The earlier v21 report of 7 FPS was corrected by the user: it was a brief peak, with sustained speed below 5 FPS. There is no controlled same-scene benchmark demonstrating a specific v24 wall-time speedup.

Run is core 0 emulation/finish elapsed time; Draw is core 1 render elapsed time and can overlap Run; Copy is presentation afterward. Interrupts can contribute to the affected core's elapsed timings. Do not add Run and Draw as if they were sequential exclusive CPU costs. VBskip is the percentage of guest cycles bypassed in recognized VBlank loops, not hardware utilization.

The HSTX monitor signal is 60 Hz even when only five new guest frames complete per second. The latest result demonstrates a running experiment, not real-time gameplay.

## Native v24 regression checks

The retained run uses 6000 frames, the local replacement BIOS, scripted inputs, firmware-like 1024-cycle slices and a real host render thread. It compares compressed-ROM, accelerated, silent-APU execution with synchronous, original-ROM, unaccelerated, synthesis-enabled execution.

At slice/frame boundaries it checks full CPU/banked state and flags, exact guest cycles, bus/prefetch/latch state, EWRAM/IWRAM and framebuffer. Twenty PC/hash/input-read checkpoints match the preserved v23 run. Final results:

| Native result | Value |
| --- | ---: |
| Emulated frames | 6000 |
| Final PC | `080bcca6` |
| Final framebuffer hash | `5ba30470` |
| Guest cycles | 1685292894 |
| KEYINPUT reads | 11861 |
| Renderer scanline jobs completed | 959978 |
| VBlank guest cycles bypassed | 347342144 (20.61%) |
| SRAM ROM cache hits | 117386291 |
| SRAM ROM cache misses | 187416 |
| Backing-page misses | 1048 |

The equivalent older unaccelerated sequence had 225930343 SRAM-ROM-cache hits. The reduction represents fewer callbacks from work that no longer executes, not fewer original ROM bytes or a measured hardware speedup. Backing misses were unchanged. The scripted sequence reaches language/name entry; it does not complete registration or full gameplay.

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

Earlier retained PPU optimization validation also compared 240 randomized fixtures against the independently compiled v21 renderer across modes 0-5, windows, priority ties, transparency/blending, text layouts/flips, affine backgrounds and sprites. The PPU was unchanged between v23 and v24. The native paths share substantial implementation, so they can share bugs; these are regression checks within an experimental core.

## Firmware image checks

The private v24 image compiled with Arduino-Pico 5.4.4, C++20 and `-O3`, separate translation units, 8 MiB PSRAM and no USB stack. The validation checked UF2 block magic/order/count, flash bounds, vector/stack addresses, SRAM hot-code placement, exact packed ROM inclusion, PSRAM bounds and ELF initialized-RAM bytes against UF2 contents.

| Private v24 artifact statistic | Value |
| --- | ---: |
| Arduino-reported program size | 4943188 bytes |
| Static SRAM | 223896 bytes |
| Space from static end to heap limit before runtime allocations | 284000 bytes |
| Main stack reservation | 16384 bytes |
| Initialized RAM load bytes verified | 46916 bytes |
| Physical flash address span | 5002496 bytes |
| UF2 container length | 10005504 bytes |
| Fixed PSRAM arena usage | 7832576 of 7864320 bytes |

UF2 framing makes its file length larger than the physical flash span. These are numbers from a local image containing privately supplied game data, not a firmware download offered in this repository.

## Known limits

The emulator remains slow and experimental. There is no public full build because the upstream core lacks redistribution permission. Audio output and persistent saves are absent. Single-buffer presentation can tear. EEPROM timing and compatibility are incomplete; complete game correctness has not been established. Earlier apparent game stalls and two no-start optimization regressions were not fully root-caused.

The current publication does not rerun a private ROM test in a clean public checkout: the required core and game assets are intentionally absent. The text logs record the local tests that were run. Compilation of support sketches and repository publication checks are separate from those emulator tests.
