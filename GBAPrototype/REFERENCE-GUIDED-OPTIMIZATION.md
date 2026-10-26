# Reference-guided optimization plan

Reviewed 25 October 2026, Europe/Dublin. This supplements the historical
[architecture research](ARCHITECTURE-RESEARCH.md). Sources inform hypotheses;
they do not establish a measured speedup or correctness pass.

## Verified references and their role

| Reference | Use in this project |
| --- | --- |
| [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf), sections 4.2 (SRAM), 4.4 (XIP), 12.6 (DMA), 12.11 (HSTX) and 12.14 (QMI) | Memory placement, arbitration, XIP and peripheral deadlines. Check chapter titles before relying on older page numbers. |
| [Pico SDK hardware APIs](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html) | DMA/IRQ/cache APIs and safe peripheral shutdown. |
| [Official HSTX DVI example](https://github.com/raspberrypi/pico-examples/blob/master/hstx/dvi_out_hstx_encoder/dvi_out_hstx_encoder.c) | Compare DMA completion sequencing, command expansion and interrupt placement with the installed driver. |
| [GBATEK](https://mgba-emu.github.io/gbatek/) | GBA-specific memory semantics, wait states, DMA, timers, prefetch and PPU rules; distinguish GBA sections from NDS sections. |
| [ARM7TDMI DDI 0029G](https://ww1.microchip.com/downloads/en/DeviceDoc/DDI0029G_7TDMI_R3_trm.pdf), Arm manual hosted by Microchip | Guest ARMv4T state, ARM/Thumb semantics, banked registers, exceptions and memory cycles. |
| [Tonc](https://gbadev.net/tonc/) and [hardware overview](https://gbadev.net/tonc/hardware.html) | Practical PPU, background/sprite, DMA and BIOS examples. |
| [Copetti GBA architecture](https://www.copetti.org/writings/consoles/game-boy-advance/) | High-level architectural context; detailed acceptance comes from hardware references and tests. |
| [jsmolka/gba-tests](https://github.com/jsmolka/gba-tests) | Candidate ARM, Thumb, memory, BIOS, PPU and save correctness gates. Source/build/results must be pinned before claiming a pass. |
| [mGBA test suite](https://github.com/mgba-emu/suite) | Additional correctness workloads. This verified repository is preferable to relying on the supplied unverified `endrift/gba-suite` URL. |

The official RP2350 PDF was downloaded and relevant text extracted privately
under `build/reference-documents/`. Metadata records its URL, UTC retrieval time,
1,371 pages and SHA256
`a638d6780da919162660691bfa02c6dcf7c2a0b6e9f18e2d7519c80b664a2e39`.
The browser could not load the large PDF directly; local extraction was used.
The direct Arm documentation-service URL timed out, so the manufacturer's copy
of the actual Arm manual was consulted instead. Links being reachable is not a
claim that every section or test has been reviewed.

Armwrestler, FuzzARM, the other BIOS projects, community writeups and the other
listed emulator projects remain additional leads. No new test-suite execution,
licence conclusion or measured gain is claimed for them here.

## Details that change how the recommendations apply

The current RP2350 datasheet specifies 520 KiB SRAM: eight 64 KiB banks and two
4 KiB banks. Main SRAM is word-striped in two groups. Its shared XIP cache is
16 KiB, two-way set associative, with 8-byte lines; QMI exposes two 16 MiB chip-
select windows. Physical storage capacity and mapped address capacity differ.
These facts are from sections 4.2 and 4.4 of the downloaded datasheet.

Our clock configuration is already 240 MHz CPU/30 MHz PSRAM. Further clock
changes are separate experiments requiring memory and display validation.
The emulator already exists: a from-scratch Mode 3/4-first build order is useful
background, but does not describe the remaining work.

The native backend translates both ARM and Thumb guest paths. Shared ARM
ancestry does not make the GBA program directly executable with its original
memory map, CPU modes and peripheral timing. The frozen native replay covers a
specific scripted workload; it is not evidence of arbitrary-game coverage.

The legacy backend and the new native runtime have different memory layouts.
The legacy path already placed several hot guest memories in SRAM. The native
port initially puts large runtime vectors in PSRAM. A placement improvement for
one backend must not be described as a missing feature of both.

Memory budgets must include the actual output driver. Its 320x240 RGB565 scanout
uses 153,600 bytes (150 KiB) of SRAM, independently of the guest's 76,800-byte
framebuffer. Replacing a guest full-frame output with small rows does not remove
the driver's scanout allocation. IWRAM/VRAM/palette/OAM, ROM hot-cache, stacks,
allocator metadata and audio buffers need a complete linked/runtime budget.

Host PSRAM stalls are host costs, not automatic emulation of guest EWRAM wait
states. Direct-memory and page-table paths must preserve alignment, mirroring,
open bus, MMIO side effects and guest access timing. Likewise, simplifying
prefetch or skipping timing checks cannot count as an optimization without
correctness validation. Hot code belongs in the memory tier justified by its
working set and measurements; blanket relocation of all code into SRAM will
not fit this project.

## Priorities grounded in current measurements

1. **Make scanout reliable under the real memory workload.** Test 3 moved the
   retained timing data to SRAM without altering the native runtime. It still
   changed from about 31,472 IRQ/s during loading to 84,066 IRQ/s during ROM
   metadata scanning. The framebuffer retained non-black HUD/game pixels.
   Thus timing-data relocation alone did not fix the problem. No FPS window
   was accepted; V52 restoration was verified.
2. **Instrument DMA phase and interrupt deadlines.** Record actual pending
   channel bits, busy state, maximum ISR gap/duration and FIFO state around the
   first rate transition. Compare with the installed driver and official
   example. The current expected-channel acknowledgement can lose phase after
   missed/coalesced completions; this remains a hypothesis to test.
3. **Add recovery only with explicit ownership.** Before a reset/free/rebind,
   stop the producer, drain the row queue and receive a core1 quiescence
   acknowledgement. Restart from the owning core, clear the HSTX FIFO through
   the proven peripheral-reset sequence, rebind, then resume. The SDK documents
   RP2350-E5: disable an aborted channel and chained channels before aborting.
   Count recoveries and invalidate unhealthy benchmark windows.
4. **Then measure native dispatch and memory costs.** The prepared lookup-cache
   experiment reduces search work in host replay; it has no board FPS result.
   Compare it against the same native backend, ROM/BIOS, frame window and healthy
   display. Evaluate hot IWRAM/VRAM/palette and executable-code placement with
   per-access/profile evidence and a full SRAM budget.
5. **Keep correctness independent of speed.** Pin selected public test sources,
   build the appropriate test ROMs and record explicit pass/fail results for
   ARM/Thumb, alignment, flags, MMIO, DMA/timers and PPU changes. Existing
   independent mGBA differences remain unresolved. Own-runtime endpoint parity
   is a regression check, not full hardware-accuracy proof.

Event batching, decode/condition tables, DSP span operations, SRAM-bank placement
and scanline/audio parallelism are profiled candidates after these prerequisites.
Record elapsed time, code/data cost and correctness together. HSTX is our display
peripheral; PIO is not currently generating the HDMI signal. The ESP32 remains
the controller bridge, so ESP32-S3/LCD migration is a separate hardware project.

## Test 3 evidence

Private capture:
`logs/perf-lab/native-runtime-hdmi-v3-2026-10-25_14-12-36/`.
Native image/tables and runtime construction pass. Scanout health fails before
the measured window; native execution reaches frame 10 while final diagnostics
are drained. The runner then restores V52, verifies map fingerprint 2447646605,
and records the fallback dump hash. No new FPS improvement is claimed.

## Concrete performance experiment queue after Test 3

At 7-8 FPS, wall time is 125-143 ms/frame. The 60-FPS benchmark permits about
16.7 ms. A few-percent interpreter change cannot close that gap.

| Experiment | Concrete change | Measurement / acceptance |
| --- | --- | --- |
| Clean CPU baseline | Run the native replay on the Feather with physical HSTX IRQ/copy disabled, retaining all emulated PPU/APU work | Report as a headless upper bound. Same ROM/BIOS/inputs/endpoints; separates native-runtime cost from malformed scanout. |
| Native dispatch cache | Integrate the prepared approximately 17 KiB key-to-native-function cache | Its host replay removes about 99.75% of table comparisons, not 99.75% of total CPU time. Require paired board timings and exact replay checks. |
| Hot guest memory | Explicitly allocate the native port's remaining 32 KiB IWRAM and 96 KiB VRAM in SRAM | Preserve allocation provenance and guest bus semantics. Palette/OAM/IO already use the small SRAM pool. Keep EWRAM in PSRAM initially. Check actual free heap, stack and all frame hashes. |
| Executable placement | Integrate the prepared mixed Flash/PSRAM image layout | Static closure prototype puts about 1.67 MB in Flash and 3.09 MB in PSRAM; final runtime/image/load ownership and hardware execution still need validation. Measure instruction-fetch/locality changes and full Flash fit. |
| Scheduling and renderer | Profile again after the above; batch only safe event-free execution, then target measured PPU/span costs | Check IRQ/DMA/timer boundaries and pixels. Keep native/interpreter backend and scene comparisons distinct. |

The working interpreter remains the fallback. Promote a change only when the
same workload becomes faster, state/cycle/pixel checks pass, memory margins hold
and the final display is stable. Headless gains cannot be reported as HDMI
gameplay gains. No FPS estimate is assigned to an unmeasured experiment, and
60 FPS is still an unachieved target.
