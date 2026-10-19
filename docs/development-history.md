# Development history: from controller reports to a running GBA

This chronology is reconstructed from the project conversation, retained sketches, local version archives, summaries and test logs. It covers the available development record through **19 October 2026**. Exact timestamps were not retained for each iteration, so version numbers express sequence rather than invented dates or backdated Git commits. The newest report takes precedence over earlier expectations or measurements.

## 1. Read a DualSense on the ESP32

The original goal was to connect a PS5 DualSense controller to an ESP32 and read all available values, with the intention of forwarding selected values to an RP2350 later. The identified chip was an ESP32-D0WD-V3, revision 3.1, with Bluetooth Classic. A later flash query found 4 MiB flash. That distinction mattered: the chip identifier alone does not establish the capacity of the board's external flash.

Arduino IDE was the requested development environment. The first interaction centered on a Serial Monitor stream, but it was hard to inspect changing sticks, touch data, sensors and buttons in text. A local browser dashboard was tried. It encountered a page-loading problem and then a Web Serial open failure, with the suggestion to close Arduino Serial Monitor. These were host/UI and serial-port-access problems, before the HDMI emulator work.

The browser dashboard was subsequently removed from the active data path. The user wanted optimized UART-only output rather than a dashboard dependency. The resulting binary bridge preserves the complete report and leaves application-level selection to the Feather.

## 2. UART receiver and HDMI bring-up

The first RP2350 steps were receiving controller packets and separately proving that HSTX video worked. The standalone HDMI test displayed color bars, a border, a moving square and counters. An upload failure led to documenting the Feather's BOOT + RESET UF2 procedure and distinguishing its port from the ESP32's COM port.

Once HDMI worked, the receiver displayed live controller state directly on the monitor. This verified a useful end-to-end chain before an emulator was involved: DualSense, Bluetooth, ESP32, UART, Feather and display. The controller viewer remains a recovery and input-isolation tool.

## 3. Choose emulation and solve ROM storage

The next goal was a generic GBA emulator with the controller as input. The Feather had 8 MiB PSRAM, but no SD card module. The user considered storing a game on the ESP32 and moving it over UART. The final direction was explicit: keep ROM data on the RP2350 only.

The first supplied game was Super Mario Advance 3: Yoshi's Island EU, an 8 MiB image. With firmware sharing the Feather's 8 MiB flash, a raw embedded image would not fit. Independent compressed pages and on-demand decoding allowed the complete ROM to be retained without expanding it all into RAM. A local `gba-embedded` adaptation provided ARM/Thumb and GBA hardware models; a replacement BIOS was used after HLE-only boot did not work for the initial game.

The later native-port discussion examined `pokeemerald-rp2350` as a different approach: a particular source-level game port, not a general converter for arbitrary GBA ROMs. The project stayed with the user's requested emulator route. No claim was made that a ROM can simply be converted into a native RP2350 executable.

## 4. Startup and memory bring-up: initial build through v14

The first firmware did not immediately produce a game. The recurring observation was startup text, then black or monitor "No signal." The iterations made that symptom more observable before attempting performance work.

| Iteration | Change / experiment | Evidence and outcome |
| --- | --- | --- |
| Initial prototype | Embed compressed Yoshi ROM, cache pages, boot the interpreter | Compiled and passed native loader/frame tests; hardware startup lost video |
| v2 | More visible boot stages and a separate HDMI/PSRAM diagnostic | User saw startup text, CPU 240 MHz and 8 MiB PSRAM information; video still failed |
| v3 | Reduce decompressor stack use: tables moved from stack to static SRAM | `tinf_uncompress` stack frame fell from 1632 to 48 bytes; this alone did not solve startup |
| v4 diagnostic | Isolate video and PSRAM at CPU 120 MHz / PSRAM 30 MHz; show each stage before work | User reported video loss after allocation-related stages; allocation and direct memory access still needed separating |
| v5 diagnostic | Reserve 2 MiB through the linker; test direct reads/writes before `pmalloc()` | User reported the 2 MiB direct test passed, then failure at stage 5 allocation. This localized the trigger, without proving the allocator's internal fault |
| v6 | Use a fixed, bounded PSRAM arena with SRAM allocation metadata | ROM block verification completed on hardware; later startup still failed |
| v7 | Persist watchdog/fault stage information for recovery | Reported stage 3 cartridge scan watchdog timeout, fault status `00000000`; a watchdog with zero CFSR was not proof of a CPU HardFault |
| v8 | Heartbeat and progress callbacks while scanning ROM chunks; cache scan results | Scan could be observed and serviced rather than running as an opaque long operation |
| v9 | Instrument core allocation/constructor substeps | Exposed failures around stage 26, IWRAM allocation, instead of just "construct emulator" |
| v10 | Put remaining bus buffers on the fixed allocation path | Arena budget was explicit; later last-visible stage moved to 30, VRAM allocation |
| v11 | Increase main-stack reservation from 2 KiB to 16 KiB; print stack/arena headroom | User later reached constructor complete at stage 34, with arena 1520704/2097152 and stack headroom about 16040 bytes |
| v12 | Disable USB stack/logging in GBA firmware, arm watchdog before transitions, mark setup return separately | User reached stage 36, "core returned to setup," and scan 8192 KiB; regular LED blink showed application progress despite lost HDMI |
| v13 | Detect persistent idle video DMA, preserve framebuffer and restart HSTX | Added targeted recovery and distinct recovery LED cadence; scan-completion video loss still required investigation |
| v14 | Count completed video IRQs, recover stalled output, intentionally restart video after scan/reset | User finally reported the game starting at about 2-3 FPS |

Stage 34 or 36 is a progress observation, not a proof that every constructor or allocation is fault-free. Likewise, enlarging the stack and bypassing an allocator were useful mitigations, not complete explanations for every black screen. Some runs stopped at different points, and in some failures no recovery report appeared at all.

The key distinction became: is the application stopped, or is the display stopped? The half-second LED heartbeat, persisted stages, scan callbacks and video-IRQ completion counter made that question answerable more often.

## 5. Performance, rendering and input: v15-v20

Once a game appeared, the bottleneck changed from boot reliability to throughput and apparent game stalls. User reports included 2 FPS, video IRQ rates around 31400-31500 per second, ROM decode around 69 ms/s rising to 100-200 ms/s, and a copy cost near 10 ms/frame. IRQ frequency did not by itself identify how much CPU time video consumed.

| Version | Main work | Observed result / qualification |
| --- | --- | --- |
| v15 | CPU 240 MHz with PSRAM held at 30 MHz; static SRAM bus memory, 32 KiB hot ROM cache, `-O3`, reduced HUD redraws | Booted, but user still reported roughly 1-2 FPS |
| v16 | Additional IRQ/runtime timing instrumentation and RAM-wait acceleration experiment | No HDMI signal and LED off. Startup cause not established; this build was not kept as the working baseline |
| v17 | Restore known boot/IRQ path while retaining strict RAM-poll acceleration | Booted at about 3 FPS; user reported 6-10 misses/s, decode 100-200 ms/s and reset count 2 |
| v18 | Make previously empty `IRAM_ATTR` place hot interpreter/bus/render code in SRAM; add Run/Draw/Copy diagnostics | User reported about 4.6 FPS at one point; later 2.8 FPS and apparent stuck/malformed graphics |
| v19 | Fix independently reproduced tile-cache generation wrap and 32x64 background-map indexing; retain short input edges and add PC/key/read diagnostics | Targeted old-code failures reproduced and fixed. The exact user-observed game stall was not conclusively reproduced or solved |
| v20 | Actual core 1 scanline worker, synchronization at shared video changes, larger associative decoded-ROM cache | User confirmed boot but about 3 FPS; an increasing C1 job counter did not prove visible game progress |

The apparent stall had several clues: PAD stayed ON, LED and Run continued, Draw/decode/misses could become zero, PC changed and input-read counts could stop. This was insufficient to label it a Bluetooth failure, a firmware halt or an anti-emulation check. A quoted Nintendo protection notice was later clarified to have been found elsewhere, not displayed by the game. It was therefore not treated as evidence of an on-screen protection failure.

Native tests found two concrete graphics bugs independently of that report. Twelve-bit tile-cache generation tags could accept stale data after 4096 invalidations. A 32x64 text background selected the wrong lower screen block. Targeted regressions covered both fixes. Their existence did not establish that either explained the exact physical screen the user saw.

## 6. Switch to Sonic and recover from a failed optimization bundle: v21-v23

The user supplied Sonic Advance 3 EU, a 16 MiB game, and ultimately requested Sonic for subsequent builds. Some intermediate requests returned to Yoshi during optimization discussions; preserved builds were kept, and the current selected image became Sonic.

Sonic compressed to 4753067 bytes in 1024 pages. Cartridge detection selected Flash64K. To avoid consuming scarce internal SRAM for saves, the save buffer reserved a reusable 128 KiB PSRAM allocation. The ROM backing cache gave up eight pages and settled at 440 pages, leaving 31744 bytes spare in the fixed arena.

| Version | Change | Result |
| --- | --- | --- |
| v21 | Sonic ROM, PSRAM save reservation, adjusted ROM cache; retain working startup | Hardware booted. The initial "7 FPS average" report was later corrected: 7 was a brief peak and sustained speed was below 5 FPS |
| v22 | Bundle PPU inner-loop changes, silent APU, unified core compilation, additional SRAM code placement and direct renderer output to HDMI buffer | Extensive native tests passed, but hardware had no display and no blinking LED. Exact startup cause remained unknown |
| v23 | Restore v21 startup/build/worker/presentation arrangement while retaining independently tested PPU optimizations and silent APU | User confirmed boot and roughly 4-5 FPS |

The installed compiler rejected LTO, so v22 used unified translation units rather than working LTO. Its direct-output experiment removed an extra PSRAM framebuffer trip in native tests, but that was not accepted as a working hardware optimization after the board failed to start. v23 deliberately narrowed the changes. It did not silently claim the v22 features remained active.

The retained PPU work includes text tile spans, preordered background priorities, choosing the visible layers needed for compositing, incremental affine coordinates, ordinary in-range memory reads without modulus and avoiding redundant dirty-array fills. Silent APU mode omits discarded waveform synthesis while preserving tested register/FIFO/event behavior; guest sound-driver code still runs.

## 7. Profile a real busy-wait: v24

With the sustained baseline corrected to below 5 FPS, native instruction sampling was used to find guest work rather than guessing at another broad rewrite. In a 2400-frame Sonic sequence, about 63% of executed-instruction samples fell in a Thumb block around `0x080bc1e0`, repeatedly checking DISPSTAT's VBlank bit. Instruction-sample share is not a Feather wall-time profile.

The existing RAM-wait recognizer did not accelerate that loop. V24 extended its strict pattern checks to read-only VBlank bit polling, including taken BEQ and BNE forms. It skips whole redundant iterations only before the next scheduled hardware event and preserves the final normally executed tail and timer-service phase. No ROM patch or frame skipping was introduced.

Sixteen synthetic cases covered branch polarity, prefetch, short slice boundaries, HBlank/VBlank/timer IRQs and rejection of other status masks. The 6000-frame compressed/threaded/accelerated test matched ordinary execution in full CPU state, cycles, bus state, RAM and pixels. It bypassed **347342144 guest cycles out of 1685292894**, or **20.61%**, and reduced SRAM-ROM-cache hit callbacks from 225930343 to 117386291. This reduced work did not establish a corresponding FPS multiplier.

The new HUD showed `VBskip %`. The user then confirmed the build worked and reported:

| V24 hardware metric | Latest report |
| --- | --- |
| Emulated FPS | About 5 |
| VBskip | 15-21% |
| Draw | 46 ms/frame |
| Copy | 10 ms/frame |
| Run / specific game screen | Not supplied |

V24 is a working hardware result, not the end of the performance problem. It confirms a particular accelerator is active; it does not demonstrate 30 or 60 FPS or complete game compatibility.

## 8. Reduce CPU overhead and isolate direct rendering: v25-v26

V25 kept the working startup, clocks, HDMI driver, linker and renderer while reducing repeated calls in the interpreter. Tiny IRQ, DMA-deadline, halt, prefetch and cartridge-size helpers were inlined with the same behavior. Three hot ROM-cache callbacks moved into SRAM; decompression and cache fills kept their existing path. Assembly showed 20 static call sites becoming 12 in the inspected CPU loop. That is a code-generation result, not a 40% speedup claim.

The next report was about 6 FPS, Run 144, Draw 37, Copy 10 ms/frame, VBskip 12%, ROM misses 0-8/s and decode 0 ms/s. The report followed v25 delivery but did not name the flashed version, so its association with v25 is inferred. The screen and measurement duration were unspecified.

V26 reintroduced only the direct-render pipeline from the failed v22 bundle. Core 1 renders a dirty row into 480 bytes of SRAM, converts RGB555 to RGB565 and writes the HDMI game rectangle. Core 0 no longer copies the complete game framebuffer, avoiding the PSRAM framebuffer write/read trip. The old PPU framebuffer allocation remains reserved but is stale during direct output. Clean rows preserve existing display pixels. No guest frames are deliberately skipped.

Recovery drains the worker and detaches its display pointer before freeing HDMI storage, then restores and rebinds the new target before execution resumes. Targeted tests covered pending work, forced blank, clean rows and replacement of the display allocation. A 6000-frame real-thread comparison checked every displayed RGB565 pixel and protected margin against the ordinary synchronous framebuffer path, including replacement at frame 3000. Another 240 fixtures compared the renderer with the preserved v21 implementation.

Unlike the complete v22 bundle, this isolated change booted on the board. The explicitly identified v26 report was 6-7 FPS, PAD ON, VBskip 6-12%, Draw 49, Copy 0 and Wait 26 ms/frame; ROM misses were 0 and decode 0-19 ms/s. Run and the exact scene were not supplied. Copy reaching zero confirms removal of that presentation step; conversion work now belongs to Draw. It does not mean all pixel movement disappeared or establish the cause of v22's startup failure.

## 9. Audit CPU semantics and common hot paths: v27

Instruction tests reproduced two nonempty-list Thumb transfer defects: LDMIA could overwrite a loaded base register with address writeback, and STMIA could store the wrong base value when the base appeared later in the list. The fixes passed 8160 list/base/load/store/RAM-region cases. Neither corrected case executed in the scripted Sonic sequence, so they are accuracy improvements rather than an explanation for its low FPS or earlier physical game stalls.

Performance work specialized common IWRAM/EWRAM and active GamePak-prefetch instruction reads, while retaining the ordinary reader for unusual states and peripherals. A compact value/cycle return avoids an unnecessary general access-result buffer. The earliest timer event is cached, with an inline deadline guard before entering the large timer handler. FIFO-request checks and prefetch bookkeeping stay inline to avoid small hot calls into flash. Clocks, direct rendering and the installed HDMI driver remain unchanged.

Validation compared 30000 mixed instruction fetches with ordinary reads and 60000 randomized timer operations with a separately compiled preserved v26 timer implementation. The 6000-frame Sonic comparison retained exact CPU, timing, bus, RAM and displayed-pixel parity and all 20 checkpoints. Fast fetch served about 81.7% of instruction reads in that test; coverage is not a speedup percentage. The [core audit](core-audit-v27.md) records the fixes and remaining instruction edge cases.

The user identified v27 at approximately 7 FPS average, PAD ON, VBskip 10%, Run 121, Draw 33, Copy 0 and Wait 32 ms/frame. HDMI resets were 5 and IRQ/s 31495; ROM misses/decode were usually zero with occasional spikes. The scene and duration were unspecified. The reset count is cumulative, including two planned startup resets; it is not a measured runtime reset rate.

## 10. Avoid redundant video writes and add board measurements: v28

The renderer was still completing almost every visible scanline in the scripted sequence. Investigation found coarse invalidation: repeated writes could join the renderer, dirty all scanlines and discard decoded tile rows even when stored values had not changed. V28 compares effective palette/VRAM/OAM values, respecting alignment, mirrors and byte-write rules. Unchanged writes retain guest bus cycles and bookkeeping while avoiding the shared-memory change, join and invalidation. Actual changes retain synchronization. Storage-only display-register writes avoid redundant dirty marking; their barrier and existing register model remain, and DISPSTAT handling is unchanged.

Small changed VRAM ranges now invalidate overlapping decoded tile rows, with a conservative global fallback for large or unknown ranges. Fast VRAM DMA drains the renderer before its burst and aggregates changed bytes for one invalidation afterward. Generic in-range memory accesses also avoid unnecessary modulo operations. These are local emulator changes described here in prose; the excluded core and its patches are not redistributed.

The native 6000-frame sequence counted 4,141,934 unchanged writes out of 4,196,721 video writes (98.69%) and 1,039,108 unchanged halves out of 1,154,176 storage-register half writes (90.03%). Decoded tile-row misses fell from 8,011,631 in the conservative reference to 3,848,078, about 51.97% fewer. Worker jobs changed only from 958,816 to 958,723. This establishes less redundant work in that sequence, not a corresponding FPS multiplier or elimination of most scanline rendering.

V28 also adds a startup SRAM/PSRAM access benchmark and the original HDMI handler's elapsed time in ms/s. The memory table covers 32 KiB SRAM, 32 KiB PSRAM and 256 KiB PSRAM, using sequential and dependent/scattered accesses. It measures CPU-observed cached loop costs with HDMI running, rather than raw PSRAM chip latency or contention during active emulation. No board table values have yet been reported.

The latest user report is **7 FPS, PAD ON, VBskip 12%, Run 113, Draw 22, Copy 0 and Wait 14 ms/frame**. The screen and duration were unspecified. The Run/Draw/Wait readings are lower than the preceding v27 report, but FPS remains about 7 and these are not controlled same-scene measurements. Wait is included in Run, and Draw overlaps Run, so these times must not be added. V28 ROM miss/decode, HDMI resets, IRQ time and memory results were not supplied. Neither 30 nor 60 FPS has been demonstrated.

On-board PC sampling, DWT cycle/stall measurements, XIP/bus counters and controlled headless/UART-off experiments are proposed next profiling steps. Dynamic translation or a decoded-instruction cache, interpolator-assisted rendering and a different scanout backend are possible larger changes, not implemented v28 features. The existing backend interprets guest instructions; on-demand ROM decompression is not ROM recompilation. The [v28 findings and profiling plan](performance-v28.md) explain these distinctions.

## 11. Prepare and update an educational publication

The user requested a full architecture/history/debugging README and a `develop` branch in `8bitdev0x8/GameBoyAdvance`. The existing repository already had a GPLv3 file and reference documents. Those were preserved.

The emulator core's own README explicitly withholds redistribution rights. The user chose to publish the documentation and original support code while excluding the modified core. ROMs, generated game data, game-containing firmware and old core archives were also excluded. The result records what was built locally without presenting an incomplete public tree as a ready-to-build emulator release.

The main lessons are practical: isolate the smallest failing subsystem, make progress visible before optimizing, retain a booting fallback, inspect actual linker placement, compare correctness after timing changes, and separate measured hardware results from native-test expectations.
