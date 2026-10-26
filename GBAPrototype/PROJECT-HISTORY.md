# Development history: from controller reports to a running GBA

This chronology is reconstructed from the project conversation, retained sketches, local version archives, summaries and test logs. It covers the available development record through **20 October 2026**. Exact timestamps were not retained for each iteration, so version numbers express sequence rather than invented dates or backdated Git commits. The newest report takes precedence over earlier expectations or measurements.

## Origin timestamps

Windows creation metadata and the original Codex session establish distinct
milestones. Times below are Dublin local time (UTC+01:00 on these dates).

| Milestone | Dublin time | UTC | Evidence |
| --- | --- | --- | --- |
| Workspace folder created | 18 Oct 2026, 22:50:09 | 18 Oct, 21:50:09 | `<workspace>` Windows CreationTime |
| Original project session created | 18 Oct 2026, 22:50:23 | 18 Oct, 21:50:23 | Original session metadata |
| First user request recorded | 18 Oct 2026, 22:51:27 | 18 Oct, 21:51:27 | Original user-message event |
| First surviving original source file | **18 Oct 2026, 22:54:53** | **18 Oct, 21:54:53** | `DualSenseReader/DualSenseReader.ino` CreationTime |
| GBA emulator subfolder created | 19 Oct 2026, 00:09:42 | 18 Oct, 23:09:42 | `GBAPrototype` CreationTime |

The first sketch timestamp is 21:54:53.1568465 UTC; the initial root README
follows at 21:54:53.3110283 UTC. The first surviving GBA artifacts are a local
compressed ROM and its manifest at 23:09:42 UTC, followed by its README one
second later. These are observed local creation timestamps, not proof that no
earlier deleted file existed. A few generated CMake stamp rules have synthetic
2001 dates; they are not evidence of project work in 2001.

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

Validation compared 30000 mixed instruction fetches with ordinary reads and 60000 randomized timer operations with a separately compiled preserved v26 timer implementation. The 6000-frame Sonic comparison retained exact CPU, timing, bus, RAM and displayed-pixel parity and all 20 checkpoints. Fast fetch served about 81.7% of instruction reads in that test; coverage is not a speedup percentage. The [core audit](CORE-AUDIT-v27.md) records the fixes and remaining instruction edge cases.

The user identified v27 at approximately 7 FPS average, PAD ON, VBskip 10%, Run 121, Draw 33, Copy 0 and Wait 32 ms/frame. HDMI resets were 5 and IRQ/s 31495; ROM misses/decode were usually zero with occasional spikes. The scene and duration were unspecified. The reset count is cumulative, including two planned startup resets; it is not a measured runtime reset rate.

## 10. Avoid redundant video writes and add board measurements: v28

The renderer was still completing almost every visible scanline in the scripted sequence. Investigation found coarse invalidation: repeated writes could join the renderer, dirty all scanlines and discard decoded tile rows even when stored values had not changed. V28 compares effective palette/VRAM/OAM values, respecting alignment, mirrors and byte-write rules. Unchanged writes retain guest bus cycles and bookkeeping while avoiding the shared-memory change, join and invalidation. Actual changes retain synchronization. Storage-only display-register writes avoid redundant dirty marking; their barrier and existing register model remain, and DISPSTAT handling is unchanged.

Small changed VRAM ranges now invalidate overlapping decoded tile rows, with a conservative global fallback for large or unknown ranges. Fast VRAM DMA drains the renderer before its burst and aggregates changed bytes for one invalidation afterward. Generic in-range memory accesses also avoid unnecessary modulo operations. These are local emulator changes described here in prose; the excluded core and its patches are not redistributed.

The native 6000-frame sequence counted 4,141,934 unchanged writes out of 4,196,721 video writes (98.69%) and 1,039,108 unchanged halves out of 1,154,176 storage-register half writes (90.03%). Decoded tile-row misses fell from 8,011,631 in the conservative reference to 3,848,078, about 51.97% fewer. Worker jobs changed only from 958,816 to 958,723. This establishes less redundant work in that sequence, not a corresponding FPS multiplier or elimination of most scanline rendering.

V28 also adds a startup SRAM/PSRAM access benchmark and the original HDMI handler's elapsed time in ms/s. The memory table covers 32 KiB SRAM, 32 KiB PSRAM and 256 KiB PSRAM, using sequential and dependent/scattered accesses. It measures CPU-observed cached loop costs with HDMI running, rather than raw PSRAM chip latency or contention during active emulation. Later video and V30 reports supplied the table values; see the continuation below.

The V28 user report was **7 FPS, PAD ON, VBskip 12%, Run 113, Draw 22, Copy 0 and Wait 14 ms/frame**. The screen and duration were unspecified. The Run/Draw/Wait readings are lower than the preceding v27 report, but FPS remains about 7 and these are not controlled same-scene measurements. Wait is included in Run, and Draw overlaps Run, so these times must not be added. V28 ROM miss/decode, HDMI resets, IRQ time and memory results were not supplied. Neither 30 nor 60 FPS has been demonstrated.

On-board PC sampling, DWT cycle/stall measurements, XIP/bus counters and controlled headless/UART-off experiments are proposed next profiling steps. Dynamic translation or a decoded-instruction cache, interpolator-assisted rendering and a different scanout backend are possible larger changes, not implemented v28 features. The existing backend interprets guest instructions; on-demand ROM decompression is not ROM recompilation. The [v28 findings and profiling plan](PERFORMANCE-v28.md) explain these distinctions.

## 11. Prepare and update an educational publication (initial documentation checkpoint)

The user requested a full architecture/history/debugging README and a `develop` branch in `8bitdev0x8/GameBoyAdvance`. The existing repository already had a GPLv3 file and reference documents. Those were preserved.

The emulator core's own README explicitly withholds redistribution rights. The user chose to publish the documentation and original support code while excluding the modified core. ROMs, generated game data, game-containing firmware and old core archives were also excluded. The result records what was built locally without presenting an incomplete public tree as a ready-to-build emulator release.

The main lessons are practical: isolate the smallest failing subsystem, make progress visible before optimizing, retain a booting fallback, inspect actual linker placement, compare correctness after timing changes, and separate measured hardware results from native-test expectations.

## 12. Memory latency, BIOS and native PC sampling: V29–V33

V29 bundled CPU/bus placement and read-path changes. Native correctness checks
passed, but the Feather reported CPU HARDFAULT with fault status zero. The last
stage and cause were not established. V30 returned to the booting V28 core for
an isolated BIOS experiment. The supplied local BIOS booted; a BIOS change did
not remove the performance problem. Its exact bytes remain private, alongside
the earlier replacement BIOS and their distinct selection paths.

The memory benchmark finally provided physical evidence. The V30 dependent-read
loop reported SRAM 32 KiB **21 ns**, PSRAM 32 KiB **767 ns**, and PSRAM 256 KiB
**1,188 ns** per observed operation. The last value was clarified after an initial
transcription of 118. These loops ran with HDMI enabled: they are neither raw
chip timing nor proof that every frame spends the same percentage on memory.
The video handler used roughly **146–147 ms per second**, around 14.7% of one
core's elapsed time. Eliminating only that measured handler cost would ideally
give about 1.17× speed, far short of 7→60 FPS.

V31 added on-board sampling of the **native M33 program counter**, rather than
only counting guest instructions. A dithered hardware-timer interrupt captures
the interrupted PC into bounded histograms. A two-pass build maps the samples
against the exact final ELF. No allocation or text output occurs inside that
interrupt. Early samples identified bus reads, BIOS reads, interpreter execution
and renderer submission/waiting. Different five-second windows must not be
added into one percentage distribution.

V32 used scalar BIOS reads and avoided joining the renderer for unchanged
storage-only display-register writes. The user still reported poor performance.
V33 introduced a stable **256-byte SRAM instruction line**. Ordinary ROM data
reads can no longer evict the buffer under an instruction pointer; complete
instruction bounds, peripheral exceptions, guest cycles and prefetch semantics
remain. This is instruction fetching, not ROM recompilation. Reports still
placed submission around 22%, code fetch around 12%, and Wait at 24 ms/frame.

Sources: [V29](PERFORMANCE-v29.md), [BIOS trial](BIOS-TEST-v30.md),
[PC sampler](PROFILING-v31.md), [V32](PERFORMANCE-v32.md),
[V33](PERFORMANCE-v33.md), [mGBA reference review](MGBA-REFERENCE.md).

## 13. Queue the renderer safely, then make failure diagnosable: V34–V39

V34 replaced a drain-before-every-line handoff with a bounded four-job queue.
Core 0 publishes jobs; core 1 consumes them. Shared video-memory changes and
buffer lifetime changes still require synchronization. A deeper queue can
hide short scheduling gaps, but cannot make a renderer process pixels faster.
Schedule refresh also computes the next deadline in one pass.

The user reported an improvement, but the HUD showed contradictory Wait and
Qfull/Barrier readings. Investigation found a diagnostic-row ownership bug;
V34d corrected it. The bug's role in the specific transcribed readings was not
proven. This prompted matching sample numbers and later binary capture.

V35's next CPU/fetch/placement trial hardfaulted on hardware despite native
tests passing. V36 restored the V34d bus/fetch arrangement, retained the tested
Thumb ALU path and added PC/LR/SP, HFSR/SFSR and core-number recovery details.
The board booted at about 5–10 FPS. A zero CFSR alone was insufficient to
identify the cause of earlier failures.

V37 captured immutable video-register state in each queued line job. Scrolling,
window and blend-register changes therefore need not drain the renderer just
to preserve an old register value. VRAM/palette/OAM still have shared ownership
barriers. The desktop tests showed much lower explicit barrier waiting without
a significant total speedup. Hardware failed at stage 3, cartridge scanning,
before this render path was active. The fault addresses resolved to an abort
path; allocation failure was a leading explanation, not a proved original caller.

V38 replaced the scan's temporary 16 KiB allocation with a fixed 1 KiB buffer
and overlap, and recorded future abort callers. It passed scanning, then the
board aborted at stage 5, BIOS loading. This time the retained caller identified
`operator new` after a null allocation. V39 added checked BIOS storage: copy to
internal SRAM only when an 8 KiB heap reserve can remain; otherwise borrow an
immutable flash view of the same BIOS. The board ran again. The reported 5–12
FPS and Wait 8 / Qfull 8 / Barrier 0 ms/frame established operation, not a
controlled sustained improvement.

Sources: [V34](PERFORMANCE-v34.md), [HUD correction](DIAGNOSTICS-v34d.md),
[V35](PERFORMANCE-v35.md), [V36](RECOVERY-v36.md),
[V37](PERFORMANCE-v37.md), [V38](RECOVERY-v38.md), [V39](RECOVERY-v39.md).
The [PicoDMG comparison](PICODMG-REFERENCE.md) also explains why a fast original
Game Boy emulator does not establish achievable GBA speed on this hardware.

## 14. Replace handwritten HUD readings with retained evidence: V40–V41

V40 split core-1 rendering into background drawing, objects, composition,
preparation, RGB conversion and other work. The user observed approximately
4–20 FPS in different scenes. Their copied timings came from different windows,
so a single frame-time partition could not be reconstructed from them.

V41 added a **24 KiB binary PSRAM ring**. Every completed game frame records
performance counters; periodic records retain CPU profiles. Sparse encoding
keeps this small enough to coexist with ROM and working buffers. The oldest
records are overwritten, so a capture is a retained window rather than a full
play-session trace. Data is volatile and must be exported before reset.

The PC requests a USB serial export, temporarily pausing the game, validates the
framing, decodes CSV/JSON, and generates an offline browser chart. Captures now
save under `logs` with local timestamps, avoiding accidental overwrite of a
previous `gba-log.bin`. The ESP32 controller UART is separate from this USB
debug channel. Decoder tests check incomplete/corrupt records, wrapped counters,
version/map association and timing partitions.

This changed the development method: analyze matching frame records and actual
sample windows; distinguish queue-full waiting from shared-state barriers;
deduplicate overlapping exports; retain the exact ELF map. Native and board
measurements remain separate.

Sources: [renderer profiling](PROFILING-v40.md), [binary log format and workflow](DEBUG-LOG-v41.md).

## 15. Compositor and CPU-fetch experiments: V42–V44

V41 logs placed composition at roughly 53–73% of measured rendering in selected
captures. V42 introduced window spans, preselected background rows and packed
layer information; it avoids searching for an unnecessary second layer when
alpha blending cannot use one. Controlled desktop runs reduced rendering time
by about 21–22% and total time by 6–9%. Those host figures are not RP2350 FPS.

V42 board logs then placed CPU dispatch and instruction fetching ahead of
renderer waiting in slow scenes. V43 specialized constant-width ARM/Thumb code
fetches and simplified instruction-window hits. Controlled host runs reduced
CPU-only time by 3.4% and total time with rendering by 1.8%. A speculative
flag-update change was rejected rather than bundled into the candidate.

V44 specialized ordinary opaque compositor spans. Its small host differences
were scene dependent, including a regression. It also tested bounded predecoded
Thumb ALU blocks: **3,328 bytes** of entries, architectural fetch/timing retained,
no native JIT. That prototype averaged only **1.37 instructions per entry**.
Lookup, validation and entry costs outweighed saved decode work; measured
CPU-only host time rose around 8–11%. Blocks therefore default **off**, with USB
commands and log flags for an explicit board A/B trial. A failed narrow block
prototype does not prove all native translation designs would fail.

Seven V44 exports contain **1,735 unique completed frames** after exact duplicate
removal, covering **160.738 seconds** of retained activity. Their weighted rate
is **10.79 FPS**; the five longer windows range from **6.90 to 19.85 FPS**. These
are different retained game scenes, not one sustained session or a controlled
V43/V44 comparison. Earlier "no signal" recovered; these captures do not
identify its cause. The reset counter remains five in every retained frame.

The latest slow V44 window reports Run **138.09**, Draw **35.23**, Wait **13.96
ms/frame**; BG **15.04** and Mix **14.26 ms/frame** dominate Draw. Queue-full
waiting contributes **13.66 ms**, explicit barriers only **0.30 ms**. Decode
averages **3.89 ms/s**, so compressed-ROM decoding is not the principal cost
in that window. Partial top-three PC windows put dispatch and specialized
fetch ahead of the other recorded functions; this is not a complete CPU profile.

**Run includes Wait, and Draw overlaps Run. Never add those three values.**
The 60 FPS target remains unmet. Generic ARM7 interpretation, address/bus/timing
work, rendering and HDMI preemption all remain material. Sound synthesis is
muted, but the guest's own sound-driver instructions and required device timing
still run. Neither UART nor compression has been established as the dominant
bottleneck in these recent logs.

Sources: [V42](PERFORMANCE-v42.md), [V43](PERFORMANCE-v43.md),
[V44](PERFORMANCE-v44.md), [audited V44 board captures](V44-BOARD-RESULTS.md).

## 16. Make physical memory understandable

The user requested a disk-usage-style map of ROM, code and data. The interactive
explorer separates **520 KiB internal SRAM**, **8 MiB PSRAM**, **8 MiB flash** and
**8 KiB OTP**. Sources are labeled: exact build symbols/size, source-defined
allocations, board free-heap observations, residual accounting or unknown
occupancy. It is a snapshot, not a live USB memory reader.

The UI moved from four small maps to one large treemap with memory tabs,
breadcrumbs, Back, wrapped labels, an exact allocation list and measurement
notes. Small allocations remain selectable in the list. Color identifies the
kind of allocation, while byte count and source text preserve the meaning
without relying on color alone. Each memory tab has its own area scale.

The allocator audit corrected an earlier architectural claim: the active
320×240 RGB565 HDMI framebuffer occupies **153,600 bytes of internal SRAM**,
28.8% of the chip's SRAM. The explicitly allocated recovery backup is in
PSRAM. Ordinary driver `malloc()` does not become `pmalloc()`. Latest V44
captures report only **11,088 bytes of free heap**; this is a sample, not peak
stack measurement or a guaranteed minimum. A second full video-memory image
cannot simply be allocated there.

The packed Sonic ROM is **4,753,067 bytes** in flash. Decoded ROM backing pages
occupy **7,208,960 bytes** of PSRAM. The **10,159,104-byte UF2** is a transport
container, not that many flash payload bytes: its block headers/padding and
256-byte payloads explain why its file size exceeds the physical 8 MiB flash.
OTP occupancy and the PSRAM area outside the project arena were not measured
and are labeled unknown/reserved, rather than free.

Sources: [memory placement](MEMORY-LAYOUT.md), [explorer and UX](MEMORY-EXPLORER.md),
[rendering opportunities](RENDER-OPTIMIZATION-MAP.md).

## 17. V45 renderer and CPU-burst work in progress

The next local candidate tests packed RGB555 blending/brightness arithmetic
and text-background rows that write their transparent sentinel directly,
avoiding a redundant clear. Exhaustive brightness tests and randomized alpha
tests preserve rounding, saturation and bit-15 behavior. No new render cache,
second video-memory image or larger queue is introduced.

An uncached bounded Thumb ALU burst and a closed-form common prefetch-state
transition were also tested. Full architectural fetches and per-instruction
timing remain. The CPU burst does not show a dependable desktop gain and
defaults **off**. It can be enabled explicitly for a real-board trial; it is
not a native-code translator or a promised large speedup.

Three rotating-order desktop repeats show about **10.78% less Draw time and
3.73% less total time** in one renderer-only replay, and **4.21% less Draw /
1.12% less total** in a later replay. These are MSVC host results. The CPU-only
variant is approximately neutral to slower. All variants have matching guest
cycles, PC and state/output hashes for the measured workloads.

V45's local packaging now passes UF2/vector/stack/ROM/BIOS/map checks and the
6,000-frame regression. The firmware is `output/GBAFeather-Sonic-v45-USB.uf2`;
its 1,037 mapped functions match the exact final ELF. ARM static SRAM is 245,700
bytes, 1,448 more than V44; the emulator object is 86,920 bytes. These totals
are not runtime free heap. The [V45 report](PERFORMANCE-v45.md) and its JSON
summary retain checksums and details. **V44 is the latest board-measured build**;
V45 board FPS is unknown. Do not label desktop percentages as measured Feather gains.

## 18. Record the AI development process without inventing a bill

The original project session begins on 18 October 2026. Its local usage records
and five historical helper sessions can be audited without exporting prompts,
private game data or account credentials. [AI-PROJECT-METRICS.md](AI-PROJECT-METRICS.md)
defines the fixed cutoff, token categories, response-ID deduplication, elapsed
task time and current-rate API cost estimate. The machine-readable JSON/CSV and
the audit script let a blog reader reproduce the arithmetic.

Cached input is a subset of input; reasoning output is a subset of output.
Neither is added twice. Repeated conversation context explains the large
input count; it does not mean hundreds of millions of unique words were
written. Cumulative counters are never summed. Helper time overlaps primary
time, so wall time uses the union of intervals. Automatic review telemetry is
reported separately because its model price/billing attribution is unknown.

## 19. V45 gameplay evidence and CPU-focused investigation

Seven V45 captures match the exact build map and validate their time partitions.
After duplicate removal they retain 2,213 frames over 185.672 seconds. Faster
entry/menu windows coexist with slow gameplay; the two latest windows are
7.89 and 7.24 FPS. The latter averages Run 131.48, Draw 35.76 and Wait 14.70
ms/frame. About 89% of Run is outside explicit renderer waiting. ROM decoding
uses only 0.34% of elapsed time there, while the HDMI handler uses about 14.7%.
The captured PC top-three list still puts dispatch and Thumb fetching first.

The user explicitly retained smooth 60 FPS gameplay as the benchmark and asked
for deeper CPU execution/bus/scheduler analysis. The V46 source experiment tries
an inline leaf for cached instruction fetches, retaining the ordinary path for
misses, aliases, peripherals and unusual states. Its synthetic correctness audit
passes, but full packaging and board speed are not established at this checkpoint.
More complete native/guest workload profiling and controlled board A/B remain
necessary. See [V45-BOARD-RESULTS.md](V45-BOARD-RESULTS.md).

The publication update records completed work through V45 and explicitly labels
V46 as unfinished. It follows the earlier decision to publish documentation and
original support code while excluding the modified core and private game assets.

## 20. V46 candidate validation after the publication checkpoint

The V46 inline fetch path passes a 6,000-frame replay with the same twenty
CPU/video checkpoints and handles 79,003,085 cached fetches. Its first host
timing comparison is modest and mixed: 3.07% less CPU-only time, 1.60% less
total time in one rendered sequence, and 3.43% more in a later sequence.
Those sequences are not the reported slow gameplay scene and cannot establish
board speed. The engine remains runtime-switchable for a controlled board A/B.

The larger diagnostic change retains every nonzero mapped native-PC sample bin,
instead of only three rounded percentages. Six ID/count pairs fit each sparse
binary record; start/end markers detect incomplete overwritten windows. It
reuses the existing histogram and log ring, with a small fixed chunk writer.
Serialization happens outside the sampling ISR. The updated decoder exports
exact full function names/counts and marks contaminated or incomplete windows.

Fourteen protocol tests, native writer boundary tests and the full replay pass.
The final 1,038-function map and UF2 layout checks pass; V46 is packaged locally
with V45 preserved. Static SRAM rises 1,112 bytes. Board boot, runtime overhead
and FPS gain are unmeasured. The 60 FPS target remains unmet; the new evidence
is intended to locate the remaining dispatch/bus/device work before larger
execution-backend changes. See [PERFORMANCE-v46.md](PERFORMANCE-v46.md).

## 21. V46 board evidence and transition stutters

Seven new captures match the exact V46 map and pass all frame partitions.
They contain 1,990 unique frames after removing 184 duplicates, spanning
139.320 retained seconds across mixed scenes. Their averages range from
8.10 to 23.43 FPS. The user reports about one FPS gained, but identifies the
latest 11.17 FPS capture as a menu/transition rather than slow gameplay.
No controlled gameplay A/B improvement is inferred from these different scenes.

The complete histograms add 25 eligible unique windows and 63,423 native
samples. CPU-loop work is the largest sampled function, followed by renderer
submission and generic bus reads. Its inline fetch/ALU work prevents treating
the whole function as dispatch overhead. HDMI remains about 14.7% by its
separate elapsed timer; these interrupt-biased sampled shares cannot simply
be added to that timer. Free heap stays at 8,528 bytes and the recorded HDMI
reset counter stays at three.

The latest menu/transition averages Run 85.08, Draw 28.10 and Wait 17.38
ms/frame, leaving 67.70 ms of Run outside explicit renderer waits. ROM decode
is only 1.58% on average there, yet the worst 156.91 ms frame spends 54.97 ms
decoding four missing pages. Sustained CPU throughput and transition stutter
are separate optimization targets. This analysis changes no firmware;
same-scene fetch on/off/on and finer CPU-loop/guest-region attribution are
the next evidence needed. See [V46 board findings](V46-BOARD-RESULTS.md).

## 22. V47 CPU memory paths and packed flags

The next user request was to optimize performance. Rather than changing the
renderer again, this trial targets generic CPU RAM-read dispatch, BIOS code
fetch and arithmetic flag bookkeeping. Fixed-width leaves preserve alignment,
RAM mirrors, cycles, pipeline/open-bus latches and protected/peripheral
fallbacks. NZCV updates use one CPSR write and 32-bit carry/overflow formulas.
A USB `memory on/off/status` command and binary flag 256 support board A/B;
memory off retains packed flags, while V46 is the full firmware fallback.

The expanded audit passes 16 groups, including 199,200 arithmetic instructions
against an independent 64-bit oracle, 372,000 RAM/BIOS bus fixtures and 12,000
actual CPU RAM-load comparisons. A 6,000-frame replay matches all twenty V46
CPU/video/key-read checkpoints and 953,015 worker scanlines. ARM compilation,
exact map and UF2 layout checks pass. Static SRAM rises only 280 bytes; no
large buffer, clock change or renderer ownership change is introduced.

Controlled host timings remain small and mixed: the complete candidate costs
0.57% and 1.81% more in two sequences and 2.29% less in the later one. These
are entry/menu workloads, not the slow gameplay scene or board FPS evidence.
V47 is packaged as a bounded board trial, not a claimed speedup or 60 FPS fix.
Same-scene memory on/off/on captures are the next test; see
[PERFORMANCE-v47.md](PERFORMANCE-v47.md). Core changes/assets remain private.

## 23. Windows ROM uploader and independent application versions

The user next requested a frontend to choose a ROM from the computer and
upload it by selecting the Feather's COM port, then asked to retain frontend
and backend version histories for educational documentation. The local
implementation uses Windows Forms for selection/progress and a Python backend
for isolated packing, Arduino compilation, image checks and USB operations.
The game is linked into a new firmware image. Working Sonic assets and
earlier firmware remain available; no SD module or UART ROM transfer is added.

Frontend 1.0.0 and backend 1.0.0 use JSON event protocol 1. Their canonical
versions, per-job records, validation and future maintenance rules are in the
[application history](ROM-UPLOADER-HISTORY.md); the [guide](ROM-UPLOADER.md)
explains use and architecture. These versions are independent of emulator V47.

A Yoshi's Island build-only integration passed packing round-trip, both ARM
build passes, exact profile-map checks and embedded-game verification. The
result uses a 3,354,624-byte flash extent and was **not flashed**. Initial UI
inspection recognized COM9 as the Feather. The user stopped Computer Use
before the corrected layout could be verified; physical reset/upload remains
unverified. Native hot-block translation remains pending, with no claimed
gameplay performance improvement from this application work.

## 24. V48 native hot-block translation

The user explicitly asked to implement native translation after establishing
that V47 still used the interpreter. The first implementation emits Thumb-2
machine code into internal SRAM and keeps the interpreter for unsupported
instructions and execution regions. Low guest registers remain in host
callee-saved registers across ALU, mapped loads/stores and branch backedges.
Timing gates preserve guest fetch costs, DMA/IRQ handling and service cadence.
The code cache reuses the disabled predecoder's allocation budget.

Local correctness testing runs emitted ARM bytes in Unicorn against the real
interpreter/bus core. It is separate from a longer host-semantic-body replay
of the translator's admission and timing logic. A further freestanding ARM
benchmark was added because passing functional tests alone does not show
better performance.

That benchmark rejected the first per-instruction callback design: it executed
6–10% more host instructions on two synthetic loops. Batching alone remained
insufficient. The implementation was expanded to cover all low-register Thumb
ALU operations and native loop backedges. A generation-checked immutable-ROM
fetch leaf then removed repeated physical opcode lookup while retaining guest
prefetch/cycle/latch transitions. The final local loops execute 37.83% and
29.16% fewer host ARM instructions than the interpreter, with matching guest
registers/flags/cycles. These percentages do not establish board FPS gains.

Debugging caught and corrected a deadline-boundary prefetch update. Cache
churn in the initial fixed-slot layout motivated a variable-length code pool.
A separate long-run Unicorn cache issue required periodically recreating its
host execution engine. The ordinary interpreter is still available through
USB `native off`, and V47 remains the complete firmware fallback.

During this work, the user shared [Copetti's GBA architecture analysis](https://www.copetti.org/writings/consoles/game-boy-advance/),
whose Sonic Advance 3 graphics illustrations provide useful renderer context.
See [PERFORMANCE-v48.md](PERFORMANCE-v48.md) for architecture, exact testing
scope, board A/B commands, limitations and retained evidence. V48 is a local
trial; there is no measured board speedup or 60 FPS result yet.

## 25. V49: the emulator runs while HDMI is black

The user reported a black screen after flashing V48 and clarified that the
monitor retained its signal but showed no debug text. Two exact-map binary
captures show continued emulated frames, with the longer capture reaching
Sonic ROM code and sampling the generated native-code marker. The HDMI
interrupt rate was about 84 kHz rather than the working stream's 31.5 kHz;
the handler consumed about 39% of elapsed time. Both captures show PAD OFF.

Inspection found that driver shutdown stops DMA and disables HSTX without
resetting its FIFO. V49 adds a full HSTX peripheral reset before restart,
plus recovery for persistently abnormal line rate. The previous busy/progress
checks could not detect this running but malformed stream. The native CPU
implementation is retained. A USB video-status/restart helper makes future
black-screen investigation possible without reading the HDMI HUD.

At packaging, the FIFO explanation remained a working diagnosis. Subsequent
V49 board captures return to 31.5 kHz IRQ/s and 147 ms/s of handler time, with
a stable restart counter. The user confirms visible HDMI and identifies the
latest capture as active gameplay, averaging 10.56 FPS across 312 retained
frames. CPU execution remains limiting: Run outside explicit renderer waits
averages 81.70 ms/frame. Native admission/emission consumes 13.87% of complete
core-0 samples, motivating cache reuse and exit-reason profiling before further
translation work. This is not a controlled same-scene gain over V47.
See [V49 board results](V49-BOARD-RESULTS.md).

The logs also correct a residency assumption: both V48
and the V47 gameplay reference report a flash BIOS view, not an SRAM BIOS
copy. See [V49 findings and tests](PERFORMANCE-v49.md). No gameplay speedup
is inferred from these mixed startup/menu captures.

## 26. V50: board A/B rejects native as the default

The new interpreter/native/interpreter helper replaced another speculative
optimization bundle with an explicit board comparison. The user completed
the 19:40:17 local-time trial: 7.515 FPS interpreter, 5.314 FPS native and
6.875 FPS interpreter. The two interpreter runs drifted 8.89%; native lost to
both. Rendering remained near 36 ms/frame, HDMI timing was normal and ROM
decompression was zero. The controller was uniformly disconnected with zero
input, so this tests CPU throughput rather than input response.

Native counters showed 61,183 compilations in 30.078 seconds, 3.36 guest
instructions per native call and 6.37 calls per compilation. This did not
justify the native-on default. The agent switched the current Feather to
`native off` and received an acknowledgement; V50 makes that selection the
startup default while retaining the experimental translator and HDMI recovery.
The V50 core sources are unchanged. Its UF2 still needs a separate boot test.

The first trial manifest is preserved, including its conservative PAD warning.
The helper now distinguishes uniform zero-input disconnection from mixed or
partial connection changes. See [V50 findings](PERFORMANCE-v50.md). The
remaining interpreter speed is still far short of 60 FPS, and another native
candidate must beat that measured baseline before becoming the default.

## 27. V51: remove disabled-engine checks from the interpreter loop

After flashing V50, the user supplied four captures and identified the last
as demo play. It averages 7.46 FPS across 267 frames/35.78 seconds, with PAD ON,
native OFF and healthy HDMI. Run outside explicit renderer waiting remains
109.83 ms/frame, confirming that the interpreter workload still needs attention.
The other captures have different workloads and cannot serve as gains in that
same scene.

V51 selects the CPU engine once per slice. A compile-time ordinary-interpreter
specialization removes native/block/burst checks and bodies from that loop;
the optional experimental loop remains available in cached flash. Guest timing
and instruction semantics are retained. This saves 1,496 B of linked SRAM.

The preserved-V50 host comparison shows about 4% less elapsed time, and two
actual M33 synthetic loops execute 4.2–4.5% fewer host instructions. The full
6,000-frame correctness replay, twenty reference checkpoints, existing core
audit and emitted-native fixture suite pass. These results support a bounded
board trial, not a claimed large FPS gain or a 60-FPS solution. V50 remains
the fallback. See [V51 measurements and test scope](PERFORMANCE-v51.md).

## 28. Architecture research after V50/V51

The user requested a deeper comparison of the RP2350 and GBA hardware to find
a route from 109.8 ms outside renderer waiting toward the 16.7-ms budget.
The research clarifies that this elapsed bucket also includes interrupts,
memory stalls and hardware service; it is not pure CPU instruction execution.
It consults Arm's CPU manuals, the RP2350 datasheet and SDK, mGBA source,
DMA/HSTX examples, the native Emerald port and an experimental static GBA
recompiler. The proposed large change is a measured hybrid backend: translate
hot immutable ROM code on the PC, preserve interpreter fallback, reduce helper
and timing overhead, and measure coverage plus real-board memory stalls.

This is a research result, not an implemented replacement backend or a claim
of reaching 60 FPS. The findings, source links, optimistic budget model and
acceptance criteria are in [architecture research](ARCHITECTURE-RESEARCH.md).
The V51 package is validated locally and remains unflashed by the agent.

## 29. Automate hardware experiments and fix the reset path

The user authorized automated tests, uploads, log capture and further performance
work. The new performance-lab 1.0.0 CLI joins host correctness checks, isolated
builds, USB-serial-specific upload, boot/telemetry checks, timestamped charts
and bounded A/B/A trials. It preserves the pre-reset ring and a known baseline,
holds an OS lock, and records failed restoration instead of silently retrying.

The first actual V51 smoke exposed Windows COM disappearance during a successful
1200-baud touch and V51's intentional-reset recovery screen. Backend 1.0.1 fixes
reset-error classification through a pinned BOOTSEL probe; V52 acknowledges a
drained `prepare flash` command before clearing the interrupted-run marker, with
five-second rearm if upload is abandoned. No emulator-core files changed.

The initial failure remains documented. V51 was recovered using software only,
V52 was bootstrapped through its inherited marker, and a subsequent normal V52
smoke passed without a recovery detour or physical buttons. The 15 workflow,
13 uploader and 9 benchmark tests pass; the real automated host-check stage also
passes the core audit and 6,000-frame replay. V52 stays on the Feather.

The hourly GBA performance lab follow-up is active, with one bounded candidate
trial per wake and quiet behavior when nothing changes. Timed boot windows do
not provide deterministic game/input replay; candidate promotion remains off.
The newest V51 slow observation is 8.86 FPS versus V50's 7.46, an observed 18.8%
difference that cannot be attributed entirely to code. See [FPS status](PERFORMANCE-STATUS.md),
[V52 evidence](PERFORMANCE-v52.md) and [automation](PERFORMANCE-AUTOMATION.md).

## 30. Continuous optimization and workload correction — 20 October 2026

The user requested parallel investigation and a continuously running loop toward
60 FPS, then explicitly asked to retain every test, improvement and log. The
hourly heartbeat was removed in favor of an active optimization goal. CPU,
memory and renderer experiments run in isolated folders; one coordinator owns
the pinned Feather serial port and uploads. The V52 fallback stays retained.

Screenshots exposed a measurement error: zero input waits at Select a Language;
the old scripted test reaches name registration. Previous host timings remain
valid for those menu workloads but cannot establish gameplay speed. The user
accepted improving the approximately 14-FPS screen. A new verified pulse sequence
also reaches moving city demo play at frames 5000–5180, allowing separate tests.

The new fixed-frame harness clears logging at the chosen guest frame, captures
the exact interval, hashes CPU/RAM/native video endpoints and freezes execution
before USB export. It rejects mismatched input, frames, cycles, clocks, hashes
and overwritten records. Independent references remain necessary because endpoint
checksums alone do not prove all emulation semantics.

Two candidates passed local checks: guarded RAM writes/multiple-register transfers
and a zero-strength-brightness compositor specialization. Their combined
6,000-frame replay retains hash `5ba30470` and PC `080bc248`. A more aggressive
poll-loop tail change failed VBlank CPU/banked/IRQ parity and was rejected.
Instruction-count reductions are documented separately from physical FPS.

The real demo census revealed substantial ARM execution from IWRAM, consistent
with guest software audio mixing. Muting host APU output does not eliminate
that guest work; the prior Thumb-heavy menu profile had hidden it. A separate
original HSTX DMA prototype removes per-row CPU expansion in a semantic model,
but requires physical scanout validation before any performance claim.

[V53 evidence](PERFORMANCE-v53.md) is the run-by-run record, including raw log
locations, source/artifact hashes, failures, measurements and pending tests.
No new gain is claimed until the board completes a matched comparison. Core,
patches, BIOS, ROM and game-containing firmware remain private.

The first combined candidate failed `malloc(sizeof(Emulator))` before construction.
It was stopped, the queued demo comparison cancelled, and V52 restored. Low heap
headroom and fragmentation were suspected; no largest-block measurement proved
the exact cause. The repair reserves interpreter storage earlier and relocates
the cold host PCM buffer to PSRAM, recovering 8,184 SRAM bytes. One 16 KiB ROM
backing page funds that buffer while preserving the full 128 KiB save budget.
All 5,040 worst-case allocation orders and the 5,400-frame demo replay pass.
An obsolete hardcoded arena-fixture expectation failed first and was corrected;
both logs remain retained. The repaired real-board baseline then completes the
exact demo interval at 6.800 FPS with 17,400 bytes free heap. The optimized
memory/render comparison subsequently measures 7.039 FPS, a 3.51% increase in
the first matched pair. CPU/RAM/video hashes and guest cycles match exactly;
both captures are healthy. Draw falls 36.906 to 32.215 ms/frame and waiting
15.473 to 13.024 ms/frame. This establishes a small observed gain on that
workload, not 60 FPS or a repeat-trial confidence interval. The IWRAM-fetch
candidate then completes the same demo at 7.265 FPS (+3.21% versus the memory/render image,
+6.83% versus the repaired baseline), again with matching endpoints. These are
first matched trials, not repeat-run confidence intervals. The subsequent compact
DMA-video test visibly flickered or corrupted the picture and was stopped; no
usable FPS gain is claimed for it. Root retained the failed ring and initiated
one validated V52 restoration, verified at 22:19:37 local time. ROM-window pinning removes redundant internal
copies in a correctness-checked local replay; immutable Flash translation is
being built with runtime ROM hashing and final-ELF address verification. Their
board performance is unmeasured at this point.

## Current state and next evidence needed

The Feather boots Sonic, receives the DualSense through the ESP32/UART bridge,
renders through HSTX, and retains exportable performance evidence. Generic
emulation works experimentally, but broad game compatibility, persistent saves,
sound output, stable full-speed gameplay and 60 FPS are not established.

The strongest next result would be repeatable real-board A/B captures on one
slow scene, exact build maps, mode flags and matched time partitions. Larger
architectural work—extending V48's experimental Thumb translation, lower-overhead bus/timing
dispatch, dependency-based rendering, carefully budgeted video-memory ownership
or a different scanout backend—must be justified by the remaining CPU and
renderer costs. None should be described as completed merely because it was
discussed. The public repository remains docs/original-support only, with the
modified unlicensed core, supplied ROMs/BIOS and game-containing UF2s excluded.
