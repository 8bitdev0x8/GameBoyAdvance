> Publication boundary: the unlicensed emulator core, private assets and build artifacts are intentionally excluded. This is not a complete rebuildable emulator release. See [notices](../THIRD_PARTY_NOTICES.md).

# Experimental GBA on Feather RP2350

This is an experimental GBA interpreter build with the selected ROM embedded in compressed blocks. The current image contains your Sonic Advance 3 ROM. The ESP32 only sends controller packets. There is no SD card or ROM transfer over UART.

The complete [development history](PROJECT-HISTORY.md) covers the first
DualSense sketch through startup failures, performance profiling, binary logs,
memory visualization and current experiments. [AI project metrics](AI-PROJECT-METRICS.md)
records auditable token counts, elapsed task time and an API cost estimate;
the estimate is distinguished from actual billed spend.

The [V1–V49 performance retrospective](PERFORMANCE-RETROSPECTIVE.md) records
the observed FPS history, failed trials, measurement gaps and alternatives
to the current execution backend. It distinguishes progress from a controlled
speedup and sets out the next board benchmark before further translator work.
The [native A/B benchmark guide](NATIVE-BENCHMARK.md) provides a V49-compatible
tool for an interpreter/native/interpreter trial with timestamped captures,
translator counters and restoration of the original execution settings.

The [RP2350/GBA architecture research](ARCHITECTURE-RESEARCH.md) links hardware
manuals and implementation references, explains the limits of the 109.8-ms
measurement, and proposes a measured hybrid execution backend and bus-stall
profiling. These are research directions, not claimed board improvements.

The [performance automation guide](PERFORMANCE-AUTOMATION.md) documents the
local build/check/flash/capture workflow, serial-specific recovery, timestamped
charts and bounded A/B/A observations. Timed boot windows do not replace a
deterministic gameplay replay or automatically establish an FPS gain.

The local [Windows ROM uploader](ROM-UPLOADER.md) provides a game-file chooser,
Feather COM-port selection and isolated build/upload workflow. Its
[frontend and backend version history](ROM-UPLOADER-HISTORY.md) is maintained
independently of the emulator's firmware versions. Initial versions are 1.0.0;
build-only integration passes, while physical upload acceptance remains pending.

## Latest local build: V52 automation support

V52 is retained as the normal controller-input fallback and passes a software-reset/verified-upload/boot/logging
smoke test without physical buttons. Its core is identical to V51; it adds an
acknowledged planned-upload path and a five-second recovery rearm if the host
abandons the upload. Backend 1.0.1 handles Windows COM disappearance during a
successful reset. [V52 evidence](PERFORMANCE-v52.md) retains the original failed
trial, legacy bootstrap and successful normal smoke test separately.

The user requested continuous multi-agent optimization toward measured 60-FPS
gameplay. An active goal replaced the hourly follow-up. V53 work adds frozen
guest-frame/input benchmarks, validated memory/compositor candidates and a
separate HSTX DMA experiment. [V53 experiment evidence](PERFORMANCE-v53.md)
records tests, failures, artifacts and result qualifications; the
[FPS status](PERFORMANCE-STATUS.md) distinguishes observed versus proven gains.
The [CPU execution status](CPU-EXECUTION-STATUS.md) explains the remaining gap,
native-translation overhead and the independent CPU experiments.
The [whole-ROM recompilation experiment](WHOLE-ROM-NATIVE-EXPERIMENT.md) records
the separate desktop comparison and RP2350 storage/runtime investigation.
The [native startup debugging record](NATIVE-STARTUP-DEBUGGING.md) explains the
USB-less first boot, the linked Flash/SRAM division hazard, and the new HDMI
diagnostic build.
The [reference-guided optimization plan](REFERENCE-GUIDED-OPTIMIZATION.md)
maps the verified hardware/test references to remaining work and records the
25 October Test 3 result.
The first exact-frame Sonic demo trials measure **6.80 → 7.04 → 7.26 → 7.49 FPS**
for the repaired baseline, memory/compositor changes, IWRAM fetch, and the
resolved-color/fused renderer respectively (**+10.16% overall**, repeat trials pending). A compact HDMI DMA candidate was
rejected after visible flicker/corruption. It is excluded from usable FPS claims.
The first Flash-translated CPU image regresses to 5.47 FPS and remains
experimental. A separately validated dedicated SRAM CPU loop is being tested.

## V51 interpreter specialization

The latest V50 capture is confirmed demo play at **7.46 FPS**, with most game-
thread time outside renderer waiting. V51 selects optional CPU engines once
per slice and specializes the ordinary interpreter loop. Host replay time
falls about 4%, M33 test-loop instruction work falls 4.2–4.5%, and linked SRAM
saves 1,496 B. Board FPS is unmeasured. See [V51 evidence, checks and trial
command](PERFORMANCE-v51.md); keep V50 as the working fallback.

## V50 interpreter default

The V49 board A/B/A trial measured native mode at **5.31 FPS**, versus
**7.51 and 6.87 FPS** in the two interpreter observations. V50 defaults to
the interpreter, retains native mode as a USB opt-in, and preserves the V49
HDMI recovery. The connected V49 session has already acknowledged `native off`.
The V50 UF2 has not been flashed by the agent. See [V50 evidence, default
selection and tests](PERFORMANCE-v50.md). This removes a regression; 60 FPS
remains unmet.

## V49 HDMI recovery

The V48 board trial kept executing while HDMI was completely black. Logs
show an abnormal 84 kHz video interrupt stream, consuming about 39% of elapsed
core time. V49 adds a complete HSTX peripheral reset during video restarts
and recovery for an abnormal line rate. Native translation remains enabled.
Flash the local `output/GBAFeather-Sonic-v49-USB.uf2`. The user confirms visible
output, and both board captures restore the normal 31.5 kHz HDMI cadence.
The latest confirmed gameplay window averages **10.56 FPS**; a controlled
gain over V47 remains unmeasured. See [V49 board results and the next CPU/cache
work](V49-BOARD-RESULTS.md) and [V49 fix and diagnostic commands](PERFORMANCE-v49.md).

## V48 native translation

V48 implements a bounded Thumb-to-Cortex-M33 native translator, with native
ALU operations, common loads/stores, branch backedges, generation-checked ROM
opcode reuse and an interpreter fallback. Local ARM microbenchmarks execute
29–38% fewer host instructions on two test loops; this is **not a measured FPS
increase**. Native code correctness and game replay evidence are documented in
[V48 implementation and testing](PERFORMANCE-v48.md). Its initial board logs
include a black-display failure, so a gameplay speedup is not established.
Keep V47 and use native on/off captures after restoring visible HDMI output.

## Latest board-measured build: Sonic Advance 3, V47

`output/GBAFeather-Sonic-v47-USB.uf2` adds fixed-width CPU RAM reads,
bounded BIOS instruction fetches and packed 32-bit arithmetic flags. All
twenty V46 checkpoints match in a 6,000-frame replay; the expanded core
audit and exact ARM firmware/map checks pass. Static SRAM grows by 280 bytes.
Host results are small/mixed. Subsequent board logs confirm gameplay at
**7.59 FPS**, with 110.85 ms/frame of Run outside renderer waits. A controlled
board speedup and 60 FPS are not established. See the [V47 board findings and
CPU execution plan](V47-BOARD-RESULTS.md), [implementation and memory on/off
commands](PERFORMANCE-v47.md), and keep V46 as the complete manual fallback.

## Previous board-measured build: Sonic Advance 3, V46

`output/GBAFeather-Sonic-v46-USB.uf2` tests an inline cached instruction-fetch
leaf and exports the full mapped native-PC histogram. The exact ARM map,
firmware layout, core audit, binary protocol and 6,000-frame regression pass.
Seven board captures now confirm operation and complete mapped CPU profiles;
the user reports about one FPS gained. Their mixed-scene averages are
8.10–23.43 FPS; the latest 11.17 FPS capture is a confirmed menu/transition,
not the slow gameplay benchmark. A controlled gameplay gain and 60 FPS are
not established. See [V46 board findings](V46-BOARD-RESULTS.md) for the CPU
hotspots, queue waits, decompression stutters and existing fetch on/off test.
Read [V46 implementation details](PERFORMANCE-v46.md) and retain V45.

## Previous board-measured build: Sonic Advance 3, V45

Packed color math and direct text-row output pass native comparisons. A
separate uncached CPU-burst experiment remains opt-in because host CPU timing
does not show a dependable improvement. Seven subsequent V45 board captures
confirm operation, with the latest slow gameplay window at 7.24 FPS.
The packaged candidate is `output/GBAFeather-Sonic-v45-USB.uf2`; the exact map,
UF2 checks and 6,000-frame regression pass. See [V45 details](PERFORMANCE-v45.md);
see [V45 board results](V45-BOARD-RESULTS.md) for the CPU-bound slow-window
budget. Keep V44 as the unchanged fallback. A V46 inline-fetch investigation
is now packaged as a board-trial candidate; 60 FPS remains the acceptance target.

The [memory explorer guide](MEMORY-EXPLORER.md) documents the interactive
ROM/code/data treemap, its source measurements and its navigation. The
allocator audit confirms the live HDMI framebuffer is in internal SRAM;
its recovery backup is in PSRAM.

## Latest board-measured experiment: Sonic Advance 3, v44

V44 tests specialized opaque render spans and optional predecoded Thumb ALU
blocks. Blocks default **off** because the narrow prototype regresses host
time. Renderer results are small and scene-dependent (including one host
regression). Subsequent board captures show 6.90–19.85 FPS and 10.79 FPS
across deduplicated retained windows; an identical-scene gain is not
established. See [V44 board results](V44-BOARD-RESULTS.md). Use
`output/GBAFeather-Sonic-v44-USB.uf2` for this experiment and retain V43.
Read [PERFORMANCE-v44.md](PERFORMANCE-v44.md) for validation, A/B commands,
measurements and limits. The broader opportunities and pipeline diagram are
in [RENDER-OPTIMIZATION-MAP.md](RENDER-OPTIMIZATION-MAP.md); physical memory
and allocations are explained in [MEMORY-LAYOUT.md](MEMORY-LAYOUT.md).

## Previous CPU-fetch candidate: Sonic Advance 3, v43

Six V42 board logs place CPU dispatch and instruction fetching ahead of
renderer waiting in the slow scenes. V43 specializes ARM/Thumb fetch widths
and simplifies stable instruction-cache hits while keeping the hot dispatch
loop bounded. Controlled host measurements reduce CPU-only time by 3.4%
and total time with rendering by 1.8%; RP2350 performance is unmeasured.
This is a modest candidate, not a demonstrated solution to the 60 FPS target.

Use `output/GBAFeather-Sonic-v43-USB.uf2`; keep V42 as the manual fallback.
See [PERFORMANCE-v43.md](PERFORMANCE-v43.md) for capture analysis, selected
and rejected experiments, validation and limitations. The complete system
explanation is in [ARCHITECTURE.md](ARCHITECTURE.md).

## Previous compositor optimization build: Sonic Advance 3, v42

V41 board logs identify layer compositing as 53–73% of renderer time, with
CPU dispatch/fetch and HDMI interrupts also consuming substantial time.
V42 uses window spans, preselected BG rows and packed layers, and skips
second-layer searches when no alpha blending needs them. Native controlled
comparisons reduce rendering time by 21–22% and total time by 6–9%; board
performance remains to be measured. See [PERFORMANCE-v42.md](PERFORMANCE-v42.md).

Flash `output/GBAFeather-Sonic-v42-USB.uf2`, retain V41 as the manual fallback,
and use the same USB capture command below. Logging and offline charts are
retained; the decoder automatically selects the exact saved symbol map.

## Previous binary USB logging build: Sonic Advance 3, v41

V41 stores every completed gameplay frame's performance counters in a
24 KiB sparse binary PSRAM ring, plus periodic CPU-sampling snapshots.
Download over the Feather's USB serial port and decode on the PC to CSV:

```powershell
python .\GBAPrototype\tools\debug_log.py --list
python .\GBAPrototype\tools\debug_log.py --port COMx --output sonic-test.bin --open
```

Flash `output/GBAFeather-Sonic-v41-USB.uf2`. V40 is the unchanged manual
fallback. Logging is volatile and overwrites oldest records when full;
export before reset. Captures save to `GBAPrototype/logs` with local
timestamps, companion CSV/JSON and an offline browser chart. The game pauses
during export, then resumes. USB export is confirmed; USB and recording
overhead remain unmeasured on the board. Read
[DEBUG-LOG-v41.md](DEBUG-LOG-v41.md) for the fields, binary format and tests.

## Previous core-1 measurement build: Sonic Advance 3, v40

V40 separates Draw time into background, sprite, compositing, setup,
RGB565 conversion and remaining worker work. The renderer page and Run/Draw
row share a five-second interval and sample number. Renderer and core-0
pages alternate every five seconds. Use `output/GBAFeather-Sonic-v40.uf2`;
report the matching FPS/Run/Draw, BG/OBJ/Mix, Prep/RGB/Other and W/Q/B rows.
Read [PROFILING-v40.md](PROFILING-v40.md), including IFetch/fills definitions.

The 6000-frame Sonic regression, independent PPU/snapshot comparisons and
counter checks pass. The user reports 4–20 FPS across the game, with timings
transcribed from different intervals; a matched full stage breakdown and
controlled performance comparison remain unavailable. V41 adds binary
logging to capture that data. V39 also remains a board-confirmed fallback.

## Previous BIOS allocation recovery: Sonic Advance 3, v39

V38 passed cartridge scanning but aborted at stage 5 while copying the BIOS.
The retained caller identifies `operator new` after malloc returned null.
V39 uses a checked SRAM copy with an 8 KiB heap reserve, or an immutable view
of the same BIOS in flash if memory is insufficient. The startup line reports
`BIOS SRAM COPY` or `BIOS FLASH VIEW`; guest BIOS behavior is retained.

Flash `output/GBAFeather-Sonic-v39.uf2`; banner
`BOOT v39 - SONIC / SAFE BIOS LOAD`. Failure-path tests, borrowed/owned BIOS
read comparisons and 6000 Sonic frames pass. The board now runs, reporting
5–12 FPS and matched Wait 8 / Qfull 8 / Barrier 0 ms/frame at sample 314.
Run time, sustained average and BIOS storage mode remain unspecified. V36
stays the unchanged working fallback.
Read [RECOVERY-v39.md](RECOVERY-v39.md).

## Previous startup recovery candidate: Sonic Advance 3, v38

V37 failed at stage 3 before the renderer started. Its retained fault PC/LR
resolve to `abort -> _exit -> bkpt #0`. The temporary 16 KiB scan allocation
is the leading suspect; the original abort caller is not known. V38 uses
a fixed 1 KiB scan buffer plus overlap and captures future abort callers
directly. It retains v37's render-register snapshots.

Flash `output/GBAFeather-Sonic-v38.uf2`; banner
`BOOT v38 - SONIC / BOUNDED SCAN`. Allocation-denied scan tests and the
6000-frame Sonic reference pass. Physical-board startup and FPS remain
unmeasured; v36 is the unchanged working fallback. Read
[RECOVERY-v38.md](RECOVERY-v38.md).

## Failed rendering-pipeline trial: Sonic Advance 3, v37

V36 boots and the user reports approximately 5–10 FPS, with Run 115, Draw 38,
Copy 0 and Wait 37 ms/frame. V37 gives each queued scanline an immutable
register snapshot, so live scrolling/window/blend writes no longer drain
the worker. Changed VRAM/palette/OAM and lifecycle barriers remain. The queue
stays at four jobs; guest execution and all scanlines are retained.

Flash `output/GBAFeather-Sonic-v37.uf2`; banner
`BOOT v37 - SONIC / REGISTER SNAPSHOTS`. Correctness checks pass, including
38,400 snapshot stress scanlines and 6000 Sonic frames matching frozen v36.
Host results show far less explicit barrier waiting but no meaningful total
speedup; board FPS is unmeasured. Compare Qfull and Barrier with matching
sample IDs. The unchanged working v36 is the fallback. See
[PERFORMANCE-v37.md](PERFORMANCE-v37.md) for implementation and limits.

## Previous hardfault isolation build: Sonic Advance 3, v36

V35 failed on the board with CPU HARDFAULT and CFSR `00000000`; its cause is
unproven. The user-confirmed working fallback remains
`output/GBAFeather-Sonic-v34.uf2`.

V36 restores the exact v34d bus/fetch code and removes v35's ROM-reader
relocation, retaining only its direct Thumb ALU optimization. It adds
recovery PC/LR/SP, core number and HFSR/SFSR information using a validated
exception-frame parser. It is an isolation trial, not a confirmed fix.
Flash `output/GBAFeather-Sonic-v36.uf2`; banner
`BOOT v36 - SONIC / ALU ISOLATION`. The native 6000-frame reference and
opcode/frame audits pass. The user subsequently confirmed boot and roughly
5–10 FPS, Run 115, Draw 38, Copy 0, Wait 37 ms/frame; no controlled sustained
average was supplied. The new fault handler still needs a captured board
fault to validate it. Read
[RECOVERY-v36.md](RECOVERY-v36.md) for scope, diagnostics and fallbacks.

## Failed CPU optimization trial: Sonic Advance 3, v35

V35 adds a direct inline path for common Thumb arithmetic/shifts, separate
constant-width Thumb/ARM code-fetch entry points, and SRAM placement for
the unchanged ROM address-latch reader. It retains v34's renderer queue and
v34d's corrected diagnostics. Guest timing, BIOS, clocks, UART and full
rendering are preserved. A failed more-aggressive polling-skip experiment
was rejected.

Flash `output/GBAFeather-Sonic-v35.uf2`; banner
`BOOT v35 - SONIC / CPU FAST PATH`. The opcode/flag audit passes 262144 cases,
and the 6000-frame legacy-decoder reference test matches exact CPU/cycles/
bus/RAM and every output pixel. Static SRAM is 232296 bytes (+1504 vs v34d).
Native timing shows no substantial gain; physical-board performance remains
unmeasured. The v34d and working v34 images remain fallbacks. Read
[PERFORMANCE-v35.md](PERFORMANCE-v35.md) for the changes, rejected experiment,
measurements and validation limits.

## Diagnostic correction: Sonic Advance 3, v34d

The user confirms improved FPS with v34 but supplied conflicting total/split
wait readings. V34d fixes a diagnostic-row ownership bug: HDMI updates used
four phases while the new row used five. A single gameplay writer now owns
the rotation, and total/split timing rows show a matching sample number.
The emulator execution code is unchanged from v34. Numeric board FPS remains
unspecified. Flash `output/GBAFeather-Sonic-v34d.uf2`; banner
`BOOT v34d - SONIC / HUD SNAPSHOT`. Read [DIAGNOSTICS-v34d.md](DIAGNOSTICS-v34d.md)
for the recorded readings, the confirmed bug and measurement limits.

## Renderer-queue test: Sonic Advance 3, v34

V33 is confirmed running: the user reports submit_hook 22%, Bus code fetch
12%, CPU dispatch 8%, and Wait 24 ms/frame. FPS/Run/Draw and IFetch metrics
are unspecified, so no v33 FPS gain is established. Submit includes its
inlined renderer wait.

V34 adds a four-entry scanline queue and refreshes all six scheduler deadlines
with one minimum reduction. Changed video memory/registers retain their
render barriers. The new rotating `Qfull / Barrier ms/f` row distinguishes
queue capacity waits from explicit shared-state drains. BIOS, clocks,
UART, ROM, guest instruction/timing behavior and full rendering are retained.
The unsafe silent-event experiment is excluded.

Flash `output/GBAFeather-Sonic-v34.uf2`; banner `BOOT v34 - SONIC / RENDER QUEUE`.
The 6000-frame native reference test matches exact CPU/cycles/bus/RAM and
all output pixels, including display replacement. All 20 v33 checkpoints
match. Static SRAM is 230780 bytes (+424). The preserved v33 image is the
manual fallback. Desktop experiments suggest an incremental gain only;
v34 board FPS remains unmeasured. Read [PERFORMANCE-v34.md](PERFORMANCE-v34.md)
and [DEEP-PERFORMANCE-ANALYSIS.md](DEEP-PERFORMANCE-ANALYSIS.md).

## Previous instruction-fetch test: Sonic Advance 3, v33

The user reports no performance improvement with v32. Its newer samples list submit_hook 22%, Bus code fetch 11%, CPU Thumb 7%, CPU dispatch 8%, Bad0 and 36 unmapped samples; scene, total N and new FPS/Run values are unspecified. V33 targets the fetch path using a dedicated stable 256-byte SRAM instruction line, inlining the thin fetch wrappers and ROM-size/WAITCNT getters. Ordinary data-cache eviction cannot overwrite this line, and guest bus timing/latches still follow the existing paths. It retains the original BIOS and full rendering.

Flash `output/GBAFeather-Sonic-v33.uf2`; banner `BOOT v33 - SONIC / INSTRUCTION LINE`. Compare the same scene for 20 seconds and report FPS/Run/Draw/Wait, the new `IFetch hit / fills/s` row and PC0 hotspots. The 6000-frame native test matches all 20 original-BIOS checkpoints, with 83204183 instruction-line hits and 1910528 fills (97.76% reuse), avoiding 83.2 million hot-cache lookups. These are work counts, not a measured board speedup. Static SRAM grows 352 bytes plus a 256-byte dynamically allocated provider buffer and small cartridge fields. Read [PERFORMANCE-v33.md](PERFORMANCE-v33.md) for regression, memory and fallback details. The unchanged working `output/GBAFeather-Sonic-v32.uf2` is the primary fallback; no new public push is part of this private build.

## Previous profile-guided test: Sonic Advance 3, v32

V31 samples report Bus read 17%, CPU ARM 12% and Cartridge read_bios 12% in one window (N2561, Bad0, unmapped0); other reported windows show submit_hook 29%, Bus read 12% and Render wait 18%. These windows are not additive. Assembly confirms submit_hook includes an inlined renderer wait. Displayed zero sampler cost is rounded below 1 ms/s.

V32 replaces repeated BIOS byte reads/helper calls with checked scalar SRAM loads and skips unchanged pure display-register assignments before the renderer join. Both PPU helpers stay in flash; it does not restore v29's large SRAM placements. The working original BIOS, on-board sampler, CPU semantics, renderer, clocks, ROM/cache and UART remain unchanged. Flash `output/GBAFeather-Sonic-v32.uf2`; banner `BOOT v32 - SONIC / BIOS AND JOINS`. Board FPS remains to be measured.

Tests pass 600120 BIOS reads, 120000 BIOS bus accesses, 24030 display-register cases, existing CPU/video/DMA/timer/independent PPU checks, and 6000 threaded Sonic frames matching all 20 original-BIOS v30 checkpoints. The run bypasses 1026873 register half assignments before the join; scanline count remains unchanged. Static SRAM grows only 384 bytes over v31. See [PERFORMANCE-v32.md](PERFORMANCE-v32.md) for evidence and limits. The unchanged, board-confirmed `output/GBAFeather-Sonic-v31-Profile.uf2` is the primary manual-reflash fallback; v30 remains available too. No public push is part of this private experiment.

## Confirmed diagnostic build: Sonic Advance 3, v31

V30's supplied BIOS boots, but the user reports 5-7 FPS: Run 113, Draw 30, Copy 0 and Wait 32 ms/frame, VBskip 12%, PAD ON. HDMI reports 31507 IRQ/s and 146 handler ms/s; dependent reads are SRAM 21 ns, PSRAM 32 KiB 767 ns and PSRAM 256 KiB 1188 ns. These are interval reports without a specified scene/duration. The BIOS has not resolved the performance problem.

V31 retains that working BIOS/core and adds on-board native core-0 PC sampling. Flash `output/GBAFeather-Sonic-v31-Profile.uf2`; after a five-second collection window, report the named `PC0 #1/#2/#3` percentages and sampler diagnostics. Read [PROFILING-v31.md](PROFILING-v31.md) for display rotation, build/check details and sampling limits. This is a measurement build, with no claimed FPS gain. The user's mGBA reference is reviewed in [MGBA-REFERENCE.md](MGBA-REFERENCE.md); its direct instruction fetch, handler dispatch and event queue provide specific design candidates rather than a guaranteed drop-in speedup.

The working original-BIOS `output/GBAFeather-Sonic-v30-OriginalBIOS.uf2` is preserved as the primary manual-reflash fallback. V28 remains available too. Modified core, ROM, BIOS dump and generated firmware remain private.

## Confirmed BIOS test: Sonic Advance 3, v30

V29 failed on the board with `CPU HARDFAULT`, fault status `00000000`; its cause is not established. V30 restores the confirmed v28 core and support code, then explicitly selects your supplied 16 KiB original BIOS. Flash `output/GBAFeather-Sonic-v30-OriginalBIOS.uf2` with BOOT + RESET. The banner reads `BOOT v30 - SONIC / ORIGINAL BIOS`. The startup memory benchmark still displays for 15 seconds. The supplied BIOS passed 6000 native frames with exact conservative/optimized CPU, memory and direct-video parity. The user subsequently confirmed boot at 5-7 FPS with the measurements above.

If it fails, reflash the unchanged, confirmed-working `output/GBAFeather-Sonic-v28.uf2`. There is no automatic fallback. The existing Cult replacement header remains untouched, and `build.ps1` without `-BiosPath` continues to select it. See [BIOS-TEST-v30.md](BIOS-TEST-v30.md) for selection, rebuild instructions, validation and the private BIOS policy. No new public push is part of this experiment.

## Failed CPU/bus test: Sonic Advance 3, v29

The preserved `output/GBAFeather-Sonic-v29.uf2` displayed `BOOT v29 - SONIC / CPU AND BUS`. The user subsequently reported `CPU HARDFAULT`, fault status `00000000`. Its source archive and logs remain available for investigation; it is not the fallback. Its `Exec / Event / Wait ms/f` row alternated with ROM statistics every six seconds. Exec measured CPU execution and nested bus/timer/DMA work; Event measured the following scheduler/hardware-service phase. IRQs and waits remained nested. Board FPS for this failed build is unavailable.

V29 moves remaining instruction-fetch/shift/GamePak and PPU register helpers into SRAM, inlines the unchanged WAITCNT getter, reuses a ROM word rather than reading it again for the latch, and bypasses unchanged pure-storage display-register assignments before waiting for the renderer. Status/unknown and actual changed writes retain their joins and guest behavior. Clocks, installed video driver, ROM cache, worker, linker and guest timing are unchanged. There is no new scanout backend or JIT.

The 6000-frame real-threaded regression matches v28's 20 checkpoints, exact CPU/banked/flags/cycles/bus/RAM and every RGB565 display pixel/margin, including display replacement. It bypasses 1,033,387 register halves before the join and removes 4,150,452 duplicate ROM lookups. Focused tests pass 24030 display-register and 1292 ROM/GPIO/EEPROM cases, plus the existing CPU/fetch/timer/video/DMA/PPU checks. Native CPU Bus::read counts favor IWRAM/MMIO/ROM; EWRAM accounts for 1.84% in this particular language/name-entry sequence. Counts are not hardware time or bandwidth.

The user's v28 video supplies physical data: dependent-read benchmark SRAM 21 ns, PSRAM 32 KiB 769 ns and PSRAM 256 KiB 1187 ns per loop operation, plus approximately 147 ms/s in the HDMI handler. These confirm costs, not a standalone explanation of frame time. The video contains different scenes and FPS intervals; it does not establish a full-game average. See [PERFORMANCE-v29.md](PERFORMANCE-v29.md) for the complete table, measured limits, changes, validation and the separate DMA scanout candidate.

V29 reports 233268 static SRAM bytes and 274636 bytes between static end and heap limit before runtime allocations, 6152 fewer bytes of that space than v28. The fixed PSRAM budget and stack reservations are preserved. The confirmed v28 fallback remains `output/GBAFeather-Sonic-v28.uf2`, SHA256 `faeccac5954525e7a80ae244b054e88fc28123a503ae520189d6e714f36c73bc`. Metadata, logs and private snapshot: `output/performance-v29-summary.json`, `output/core-v29-audit.log`, `output/sonic-v29-direct-test.log`, `output/ppu-v29-test.log`, `output/source-sonic-v29.zip`. Keep modified core/game firmware and the supplied video private; no new public push is part of this build.

## Confirmed video-write and memory-profile build: Sonic Advance 3, v28

Flash `output/GBAFeather-Sonic-v28.uf2` using BOOT + RESET. Banner: `BOOT v28 - SONIC / VIDEO WRITES`. The startup stage 13 shows SRAM 32 KiB, PSRAM 32 KiB and PSRAM 256 KiB access costs for 15 seconds before the game starts. Record the SeqR/DepR/SeqW/RandW table, then FPS/Run/Draw/Wait and the HDMI handler's new `ms/s` value. The DepR results also alternate with the key/PC HUD line. Results include cache, loop and live HDMI IRQ cost; physical board results cannot be supplied by native tests.

V28 avoids unchanged video-memory writes and their renderer barriers, avoids dirty marking for unchanged storage display registers, preserves decoded tile rows outside small changed VRAM ranges, aggregates fast DMA change ranges, and bypasses runtime modulo on in-range generic memory accesses. Changed shared-memory writes retain the renderer join and all guest bus timing. Assembly inspection caught generic DMA memory helpers being outlined into flash; the VRAM comparison/store is now scalar and the source reader runs in SRAM. CPU/timer/APU, worker, guest timing, clock configuration, ROM cache, linker and installed HDMI driver are preserved. The IRQ wrapper adds handler timing.

The 6000-frame native Sonic sequence contains 4,141,934 unchanged video writes out of 4,196,721, and 1,039,108 unchanged register halves out of 1,154,176. Tile-row misses fall from 8,011,631 to 3,848,078. Worker jobs barely change (958,816 to 958,723), so the optimization preserves decoded data and avoids redundant joins rather than eliminating most scanlines. The conservative reference disables the new write/dirty optimizations. CPU/banked/flags, exact cycles, bus/prefetch/latches, EWRAM/IWRAM and every direct RGB565 pixel/margin match, including output-target replacement at frame 3000. All 20 checkpoints match v27. Additional tests cover 45003 video writes/reads, 600 DMA fixtures, 240 independent v21 PPU fixtures with sparse/large invalidation, and the existing CPU/fetch/timer checks. These are regression counts, not board FPS gains.

V27 is now confirmed working at approximately 7 FPS average, VBskip 10%, Run 121, Draw 33, Copy 0 and Wait 32 ms/frame; HDMI resets 5, IRQ/s 31495, usual ROM misses/decode zero with occasional spikes. Scene/duration were not supplied. The user subsequently confirmed v28 at 7 FPS, PAD ON, VBskip 12%, Run 113, Draw 22, Copy 0 and Wait 14 ms/frame. A later private video supplies separate memory/IRQ measurements and other interval snapshots, documented in [PERFORMANCE-v29.md](PERFORMANCE-v29.md). These are not controlled same-scene speedup measurements. The fallback is `output/GBAFeather-Sonic-v27.uf2`, SHA256 `4e3b09fbba8d621cc3c04def30c26e09eb0072bca38374ff233b71ea3a80901f`.

Read [PERFORMANCE-v28.md](PERFORMANCE-v28.md) for implementation, measurement limits, hardware acceleration candidates, UART analysis, decoded-code/recompiler options, and a proposed on-board PC/DWT/cache/bus profiling workflow. PC sampling and a new hardware-accelerated renderer/scanout are not enabled in this image. Wait remains included in Run and Draw overlaps Run; do not add them. No 30/60 FPS result is claimed. Metadata/logs/private sources: `output/performance-v28-summary.json`, `output/sonic-v28-direct-test.log`, `output/core-v28-audit.log`, `output/ppu-v28-test.log`, `output/source-sonic-v28.zip`. The modified core and ROM-containing firmware remain local and excluded from publication.

## Confirmed core-audit build: Sonic Advance 3, v27

Flash `output/GBAFeather-Sonic-v27.uf2` with BOOT + RESET. Banner: `BOOT v27 - SONIC / CORE AUDIT`. The detailed findings, reproduced defects, test coverage and remaining gaps are in [CORE-AUDIT-v27.md](CORE-AUDIT-v27.md).

V27 fixes two independently reproduced Thumb multiple-transfer base-register defects: LDMIA retains the value loaded into its base register, and STMIA stores the updated base when that base is later in the register list. The new tests cover all 8160 nonempty-list/base/load/store/RAM-region combinations. These exceptional cases were not reached by the scripted Sonic sequence, so they are not claimed as the cause of its low FPS or prior stalls. Empty-register-list and ARM transfer edge cases remain further accuracy work.

Performance changes specialize common instruction fetches, return only packed value/cycles instead of a general access-result struct, cache the earliest timer deadline and check it inline before entering the large event handler, inline identical FIFO flag checks, and force prefetch bookkeeping inline to prevent compiler outlining back into flash. Special memory/peripheral/prefetch cases retain the ordinary bus reader. V26's PPU, direct scanline worker, clocks, HDMI driver/IRQ wrapper, ROM cache and linker are preserved. The sketch differs only in version markers and enabling the optimized fetch path after reset.

Tests pass 30000 mixed instruction reads plus explicit ROM-boundary/RTC/EEPROM fallbacks, 60000 randomized timer operations against an independently compiled preserved v26 timer implementation, and 6000 Sonic frames with ordinary versus optimized fetches, full CPU/banked/flags/cycles/bus/RAM/direct-video parity and display-buffer replacement. All 20 checkpoints remain identical to v26; final PC `080bcca6`, hash `5ba30470`, cycles 1685292894 and input reads 11861. The fast path serves 89126473/109063662 native instruction reads (81.72% coverage, not an FPS speedup). Firmware assembly confirms the fetch/timer handlers are in SRAM and the inspected hot paths no longer call outlined prefetch/FIFO helpers.

V27 uses 225936 static SRAM bytes, 624 more than v26, leaving 281968 bytes below the main-stack boundary before dynamic allocations. The 16 KiB main stack, 8 KiB worker stack/canary and fixed PSRAM allocation budget are preserved. UF2 headers/order/count, flash bounds, vector/stacks/PSRAM, exact packed ROM and initialized RAM bytes against ELF pass validation. The user subsequently confirmed v27 at approximately 7 FPS, Run 121, Draw 33, Copy 0, Wait 32 ms/frame and VBskip 10%; scene/duration were not specified. V26 was confirmed at approximately 6-7 FPS, Draw 49, Copy 0 and Wait 26 ms/frame. Compare the same scene; no 30/60 FPS result is claimed.

The confirmed-working fallback is `output/GBAFeather-Sonic-v26.uf2`, SHA256 `4834c2b0a79d2b7bb5ef93bf10f8c312f1d0f5e1e66308a20b415d79367ef2f3`. Metadata, test logs and local-only sources: `output/performance-v27-summary.json`, `output/core-v27-audit.log`, `output/sonic-v27-direct-test.log`, `output/source-sonic-v27.zip`. The core and ROM-containing firmware remain excluded from public Git.

## Confirmed direct-render build: Sonic Advance 3, v26

Flash `output/GBAFeather-Sonic-v26.uf2` with BOOT + RESET. Banner: `BOOT v26 - SONIC / DIRECT RENDER`. Core 1 renders each dirty scanline into a 480-byte SRAM row, converts to the existing RGB565 format, and writes the HDMI game rectangle directly. This avoids the PSRAM GBA framebuffer write/read round trip and removes core 0's full-frame copy. The existing PPU framebuffer allocation is retained to preserve the memory budget, but is bypassed and stale during direct output; use a dedicated capture path rather than `framebuffer()` for future screenshots. Clean rows preserve their existing display pixels without waking core 1. No guest frames are skipped.

HDMI recovery records its watchdog stage, drains pending render work and detaches the output pointer before freeing the display buffer. It restores backed-up pixels and rebinds the worker before submitting more work. Startup initializes the game area to the same converted reset-white color as the old framebuffer. The CPU, PPU/APU, ROM cache, guest timing, clocks, linker, separate-unit compilation and original HSTX driver/IRQ wrapper are unchanged from v25. This isolates the pipeline change from the other changes in the failed v22 build; it does not claim the v22 startup cause has been identified.

The 6000-frame real-threaded direct-output test passes every RGB565 pixel and protected display margin against the ordinary original-ROM framebuffer path, plus full CPU/banked/flags, exact guest cycles, bus/prefetch/latches and EWRAM/IWRAM comparisons. It replaces the display target at frame 3000. Targeted tests cover forced blank, clean rows, pending-job detach and retired-target protection. Another 240 randomized fixtures compare the direct worker to the independently compiled v21 renderer across modes 0-5, windows, blending, sprites/affine, text layouts and repeated writes, including cropped framebuffer fallback. All 20 Sonic checkpoints match v25; final PC `080bcca6`, hash `5ba30470`, cycles 1685292894 and input reads 11861. Worker jobs fall from 959978 to 958816 by suppressing no-op rows, not by skipping guest frames.

The latest hardware report during this build was 6 FPS, Run 144, Draw 37, Copy 10 ms/frame, VBskip 12%, ROM misses 0-8/s and decode 0 ms/s. It followed v25 delivery but did not explicitly identify the flashed version or game scene. The user later explicitly confirmed v26 at approximately 6-7 FPS, VBskip 6-12%, Draw 49, Copy 0 and Wait 26 ms/frame, decode 0-19 ms/s and ROM misses 0. The latest scene and Run timing were not supplied. Its HUD retains Run/Draw/Copy and adds Wait, the time core 0 spends blocked on the renderer. Wait is included in Run, and Run/Draw can overlap. Conversion/presentation now belongs to Draw; Copy should be near zero. Compare the same Sonic scene after loading settles. No 30/60 FPS result is claimed.

The firmware uses 225312 static SRAM bytes, 760 above v25, with 282592 bytes below the 16 KiB main-stack boundary before runtime allocation. The 8 KiB renderer stack/canary and 7832576-byte PSRAM arena usage are unchanged. UF2 structure/flash bounds, vector/stacks/PSRAM, exact packed ROM, initialized SRAM loads and render hot-code placement pass validation. The private image's flash span is 5002752 bytes. V25 is preserved, SHA256 `e9e8f08d8761a2dac59ff400569c711c82956a5991b3cbdd7e03e2fff13a2b54`. Details: `output/performance-v26-summary.json`, `output/sonic-v26-direct-test.log`, `output/ppu-v26-test.log`, and local-only `output/source-sonic-v26.zip`. Keep the firmware and modified core archive out of public Git.

## Previous CPU-overhead build: Sonic Advance 3, v25

Flash `output/GBAFeather-Sonic-v25.uf2` with BOOT + RESET. Its banner says `BOOT v25 - SONIC / CPU HOT PATH`. V25 targets interpreter call overhead while preserving the v24 clock setup, HDMI driver/IRQ wrapper, render worker, PPU/APU, linker and separate-source build flags. No guest timing, ROM data or frame skipping changes are introduced. The last confirmed hardware baseline is v24 at about 5 FPS, VBskip 15-21%, Draw 46 ms/frame and Copy 10 ms/frame; v25 hardware performance is not yet measured.

IRQ line/IE/IF/IME/advance, DMA deadline, halted-state and prefetch bookkeeping methods now inline at their call sites. The cartridge ROM-size lookup also inlines. All moved method bodies were compared against the preserved v24 implementation, ignoring whitespace only. The firmware assembly confirms the CPU loop no longer calls out to flash for the tiny IRQ/deadline/halt/prefetch operations; static call sites in `cpu_run_until` fall from 20 to 12. This is a code-generation observation, not a measured speedup. Only the three ROM cache-hit callbacks additionally move to SRAM; cache misses and decompression retain their previous path.

The 6000-frame native regression passes full CPU/banked/flags, exact cycle, bus/prefetch/latch, RAM and pixel comparisons. All 20 checkpoints match v24, with final PC `080bcca6`, hash `5ba30470`, 1685292894 guest cycles and 11861 input reads. The same 347342144 VBlank cycles are bypassed, confirming guest work/timing did not change. Existing VBlank/IRQ, RAM wait, ROM/cache/corruption, arena, input, save, graphics, audio-state and video tests pass. The PPU and renderer are unchanged.

The compiled image uses 224552 static SRAM bytes, 656 above v24, leaving 283344 bytes below the main-stack boundary before dynamic allocations. The main stack remains 16 KiB and the worker stack 8 KiB plus canary. Firmware validation passes UF2 structure/flash bounds, vector/stack/PSRAM layout, initialized RAM bytes against ELF, exact packed ROM inclusion and hot callback placement. The working v24 fallback is unchanged, SHA256 `42c86fd17bf0f9a0e9db1c51822f99cb60235cf7c3f089e59946a4bed3f1c64b`. Native checks do not prove board startup or FPS. Compare sustained FPS and Run/Draw/Copy on the same Sonic screen. Details and local-only sources are in `output/performance-v25-summary.json`, `output/sonic-v25-native-test.log` and `output/source-sonic-v25.zip`. Do not publish the core archive or ROM-containing firmware.

## Confirmed working build: Sonic Advance 3, v24

Flash `output/GBAFeather-Sonic-v24.uf2` using BOOT + RESET. The startup banner says `BOOT v24 - SONIC / VBLANK WAIT`. V23 is confirmed to boot on the user's Feather at approximately 4-5 FPS. The earlier 7 FPS report was a brief peak; sustained performance was below 5 FPS, so 7 is not the comparison baseline. The user confirms v24 works on hardware at about 5 FPS, VBskip 15-21%, Draw 46 ms/frame and Copy 10 ms/frame (19 October 2026). The exact screen and Run timing were not supplied; 30 FPS remains unmet.

Native instruction sampling found a five-instruction Thumb loop polling the read-only DISPSTAT VBlank bit. V24 extends the existing strict RAM-wait accelerator to this pattern, allowing both BEQ and BNE. It checks stable registers, flags, bus/prefetch/latch state and loop period, then skips only whole iterations before the next scheduled hardware event, retaining a normally executed tail and the timer-service instruction phase. Only DISPSTAT bit 0 is permitted for MMIO polling; arbitrary MMIO and other status masks are rejected. No ROM patch, guest clock change or frame skipping is used.

Sixteen synthetic cases compare accelerated and ordinary execution with BEQ/BNE, prefetch off/on, HBlank/VBlank/timer IRQs, variable short event boundaries and unsupported-mask rejection. A 6000-frame Sonic test passes exact CPU/banked state, cycle, bus/prefetch/latch, RAM and pixel comparisons between threaded compressed/muted/accelerated and synchronous original-ROM/unmuted/unaccelerated execution. All 20 checkpoint PC/hash/input-read counts match v23. The final frame hash is `5ba30470`, PC `080bcca6`, guest cycles 1685292894 and input reads 11861. These comparisons use the same experimental core and do not establish original-hardware accuracy or complete gameplay.

The test skips 347342144 guest cycles (20.61%) of redundant VBlank polling. SRAM ROM cache hits fall from 225930343 to 117386291 in the same 6000-frame sequence; misses and backing-page misses remain unchanged. Neither reduction is a measured board FPS gain. The HUD's `VBskip N%` is the share of guest cycles bypassed during the reporting interval, not processor utilization or speedup. `Run`, `Draw`, `Copy` and ROM decode timings remain visible. Compare sustained FPS and VBskip on the same Sonic screen after initial loading settles. 30 FPS has not been demonstrated.

The confirmed-working v23 startup, HDMI driver/IRQ wrapper, 240 MHz CPU/30 MHz PSRAM clocks, worker, PPU/APU, separate-unit build flags and linker are preserved. Static SRAM is 223896 bytes and heap space before dynamic allocations and below the 16 KiB main stack is 284000 bytes. The 7.5 MiB PSRAM arena is unchanged. The packed Sonic ROM is verified against the UF2, with flash bounds, vector/stacks, initialized RAM load segments and hot-code placement checked. Details, native log and sources are in `output/performance-v24-summary.json`, `output/sonic-v24-native-test.log` and `output/source-sonic-v24.zip`. ESP32 firmware and wiring are unchanged. The v23 fallback is preserved with SHA256 `45a2b2b8f37f9322c77b6af6c3288334ad53cc6d50cbf6ced67942a9ae32b6e5`.

## Confirmed boot recovery: Sonic Advance 3, v23

The user reports v22 produces no HDMI output or blinking lights. Its startup regression is not root-caused. Do not use v22 as the working baseline.

Flash `output/GBAFeather-Sonic-v23.uf2` with BOOT + RESET. V23 restores v21's build flags, separate CPU/bus/timing source compilation, ROM provider, render worker, full-frame presentation, clock setup, HDMI driver/IRQ wrapper and linker. Its sketch differs from v21 only in the version/recovery marker, startup banner (`BOOT v23 - SONIC / RECOVERY`), and enabling the silent APU mode after reset. The v22 graphics inner-loop improvements remain, with pixel equivalence against v21 checked independently. The experimental direct-to-HDMI output and additional CPU/ROM SRAM placement are not in this recovery image.

The image clean-builds with 223840 bytes of static SRAM, 8000 bytes below v22, leaving 284064 bytes before dynamic allocations and the reserved 16 KiB main stack. Native checks pass 240 randomized PPU comparisons and 6000 frames of threaded compressed-ROM execution with synthesis disabled versus synchronous original-ROM execution with synthesis enabled: full CPU/banked state, cycles, bus state, RAM and framebuffer match. All 20 PC/frame/input-read checkpoints match v21. These checks do not confirm board boot or FPS improvement.

Hardware update: the user confirms v23 boots and runs at approximately 4-5 FPS. The earlier v21 7 FPS report was corrected to a brief peak, with sustained FPS below 5. Both v23 and v21 remain preserved. V21's SHA256 remains `1e39da70015b55ae51fbba0f1ad6a7e8f68280b19557873879107fea9a895880`. ESP32 firmware is unchanged. V23 sources and build checks are saved in `output/source-sonic-v23.zip` and `output/recovery-v23-summary.json`.

## Failed startup experiment: Sonic Advance 3, v22

Hardware update: the user reports no screen output or blinking lights. The original pre-test implementation notes below describe that experiment; they are not a claim that it boots.

The v22 startup banner says `BOOT v22 - SONIC / OPTIMIZED`; the FPS line also says v22. The original claim of 7 FPS average with v21 was corrected by the user: 7 was a brief peak and sustained FPS was below 5. V22 failed startup on the board; use the current image above. 30 FPS has not been demonstrated.

V22 changes the rendering pipeline: core 1 renders each dirty scanline into a 480-byte internal-SRAM row, converts RGB555 to the existing RGB565 format, and writes directly into the 240x160 game rectangle of the HDMI buffer. This removes the PSRAM framebuffer write/read round trip and core 0's full-frame copy. The old PPU framebuffer allocation is retained for compatibility/budget stability but is bypassed by this frontend; it no longer contains a current capture when direct video is enabled. HDMI reinitialization drains the renderer before freeing the display buffer and binds the new buffer before resuming. The output remains a single framebuffer and can still tear. No frames are deliberately skipped.

The renderer now processes text backgrounds in tile spans, sorts background priorities once per scanline, selects only the two visible layers needed for blending, uses incremental affine coordinates, avoids divisions for ordinary in-range video/palette reads, and avoids filling all dirty flags when they are already set. Clean scanlines no longer wake core 1. The existing graphics output is retained, including current emulation limitations.

CPU/bus/IRQ/timer/DMA/cartridge sources compile together, allowing interprocedural optimization with the installed compiler. The ELF contains a specialized Bus::read clone. Additional fetch, ROM callback, prefetch, interrupt-state and memory-access routines are linked into SRAM. The installed RP2350 GCC explicitly rejects LTO; it is not enabled. `build.ps1 -SplitCore` allows a separate-unit build; the default is the unified core. The experimental `-Lto` switch requires a different compiler with LTO support and is not used for this image.

The silent frontend omits waveform synthesis/mixing that was previously generated and discarded. Audio register/status behavior, sample/frame-sequencer event times, FIFO consumption and FIFO DMA requests are retained. Oscillator phase is not advanced in this permanently silent mode; a future audible frontend must reset before enabling synthesis. The guest CPU still executes game sound-driver instructions.

CPU remains 240 MHz and PSRAM remains 30 MHz. The HSTX driver/IRQ wrapper and linker layout are preserved. The fixed PSRAM cache/save arena is unchanged. The internal-SRAM cost rises to 231840 bytes before dynamic allocations, with 276064 bytes between __end__ and the reserved 16 KiB main stack. Core 1 retains its explicit 8 KiB stack and canary.

Validation: 240 randomized fixtures compare pixels against the independently compiled preserved v21 renderer across modes 0-5, windows, priority ties, transparency/blending, text sizes/flips, affine/sprites, cropped/full output, repeated frames and video writes. A 6000-frame Sonic run checks direct RGB565 display pixels and untouched margins against the synchronous framebuffer path on every frame, plus exact CPU/banked state, cycles, bus state and RAM. Its 20 checkpoint frame hashes/PC/input-read counts match v21; final hash is 5ba30470, PC 080bcca6, cycles 1685292894 and input reads 11861. A further 3200-frame run tests rebinding the output allocation at frame 3000. Randomized sound-register tests compare audible and silent APU register/status, FIFO-request and event behavior. ROM integrity/cache/input/save tests pass. These are native correctness checks within the experimental core, not measured Feather FPS or proof of complete game accuracy.

To reproduce the independent PPU checks, run `python tools/prepare_ppu_reference.py output/source-sonic-v21.zip --out ../build/ppu-v21-reference`, then configure the native tests with `-DGBA_PPU_REFERENCE_DIR=<absolute reference folder> -DGBA_TEST_UNITY=ON`. The main native ROM test uses `GBA_TEST_THREADED=1`, `GBA_TEST_SLICED=1`, `GBA_TEST_AUTO_INPUT=1`, `GBA_TEST_MUTE=1`, `GBA_TEST_DIRECT_VIDEO=1` and `GBA_TEST_FRAMES=6000`.

The FPS line now shows `Wait N ms`: core 0 wall time blocked waiting for rendering, averaged per frame. It is included in Run and can include interrupts. Draw includes core 1's conversion/presentation. Copy should be near zero because the copy is removed. Report FPS and Run/Draw/Wait at the same Sonic screen as the 7 FPS baseline. The old image is preserved at `output/GBAFeather-Sonic-v21.uf2`; ESP32 code and wiring are unchanged. Details and sources: `output/performance-v22-summary.json`, `output/source-sonic-v22.zip`.

Large remaining directions include a DMA-ring HDMI driver to reduce per-scanline CPU interrupts, a faster CPU backend, and further EWRAM/bus optimization. Those require separate hardware/accuracy validation; they are not implemented or claimed as FPS gains in v22.

## Earlier ROM test: Sonic Advance 3, v21

Hardware update: the user confirms Sonic runs, but corrected the earlier 7 FPS report to a brief peak. Sustained FPS was below 5. The original pre-test notes follow.

Flash `output/GBAFeather-Sonic-v21.uf2` using BOOT + RESET and copy it to the Feather's bootloader drive. The startup banner says `BOOT v21 - SONIC ADVANCE 3`. Allow the block checks, cartridge scan and HDMI relocking to finish. `output/GBAFeather.uf2` contains the same image. The ESP32 firmware and UART wiring are unchanged. Cross is GBA A, Circle is B, Options is Start, Create is Select, and the D-pad/left stick supplies directions.

The supplied Europe ROM is 16,777,216 bytes, SHA256 `d9c96aad4b3795d3e667637fcf9399636ae355fcf12677d2883107ea273dea15`. All bytes are retained in 1024 independently compressed 16 KiB pages, totaling 4,753,067 bytes. The complete firmware's physical flash address span is 4,999,936 bytes, within the Feather's 8 MiB flash. The UF2 file itself is 10,000,384 bytes because UF2 adds transfer overhead; that is not the flash capacity required.

Sonic's `FLASH512_V` signature selects a 64 KiB Flash save device. Cartridge save storage now reserves a reusable 128 KiB PSRAM allocation, sufficient for all supported save sizes, instead of exhausting internal SRAM. The decoded-ROM cache gives up eight pages and holds 440 pages (7040 KiB); the 32 KiB SRAM ROM cache is retained. The complete fixed PSRAM arena still uses 7,832,576 of 7,864,320 bytes, leaving 31,744 bytes spare. The allocator reserves the maximum save capacity once and reuses it across save-type changes. Saves remain volatile and disappear after reset/power-off.

The clean Feather build passes UF2 structure, flash bounds, vector/stack, SRAM hot-code placement, exact embedded-ROM and ELF-to-UF2 initialized-RAM checks. Native tests decode all 1024 blocks and compare 6000 frames of the threaded/compressed path against the synchronous/original-ROM path: full CPU/banked state, exact cycles, bus/prefetch state, RAM and framebuffer match. Save-storage reuse and Flash64 ID/program/sector erase/load checks pass, along with existing input, graphics, EEPROM, cache and arena checks. The script reaches language selection and name entry; the input-read counter reaches 11861 and remains responsive. It does not finish registration or establish full gameplay correctness. These are comparisons within this experimental core, not validation against original hardware.

**This is a ROM compatibility test, not a demonstrated FPS fix.** Board boot and Sonic FPS remain untested. The previous Yoshi v20 image boots on the user's Feather at about 3 FPS, with C1 and PC changing while the screen appears stuck and input reads stay at 707. The Nintendo warning quoted by the user came from elsewhere, not the HDMI screen. The cause of that stall remains unresolved. Neither 30 nor 60 FPS has been demonstrated. The recognized idle-loop accelerator does not match the Sonic test sequence, so it skips no CPU cycles in this test.

After flashing, report whether Sonic reaches language/name selection, whether Cross and directions change those menus, and FPS plus Run/Draw/Copy at a settled screen. Yoshi's previous image remains unchanged at `output/GBAFeather-v20.uf2`; its generated ROM files are preserved under `build/rom-yoshi-v20/generated` and sources in `output/source-v20.zip`. Build/test details are in `output/sonic-v21-summary.json` and `output/source-sonic-v21.zip`.

## Earlier Yoshi performance build: v20

Hardware update: v20 is confirmed to boot but runs at about 3 FPS. C1 advances; this counts dispatched scanline jobs including unchanged lines, and is not proof of game progress. The original pre-test implementation notes follow.

V20 introduces actual dual-core rendering and a much larger associative decoded-ROM cache. Core 0 runs CPU emulation, hardware scheduling, controller input, frame presentation and the existing HDMI interrupt handler. Core 1 renders scanlines. A release/acquire mailbox allows one pending scanline; CPU work on unrelated memory overlaps rendering. CPU palette/VRAM/OAM writes, LCD-register writes, direct video-DMA writes, HLE video-memory resets, emulator reset and frame presentation synchronize with the renderer before accessing shared mutable render state. Frames are drained before presentation. No frame skipping is enabled and guest cycles are unchanged.

The core 1 worker uses a separately allocated 8 KiB internal-SRAM stack with a 64-byte canary. It is launched after all startup allocations and video resets, at stage 11. The render entry and worker routines are linked into SRAM. The compiler's static stack-usage report lists 216 bytes for render_scanline, 144 for render_objects, 128 for render_text_bg, 72 for render_affine_bg and 40 for the worker; these are individual functions, not a measured peak stack depth. Hardware startup, parallel performance and stack-canary behavior remain unconfirmed.

The decoded-ROM backing cache increases from 1 MiB/64 direct-mapped pages to 7 MiB/448 fully associative pages. A page-to-slot table provides constant-time lookup; second-chance replacement avoids index collisions. The 32 KiB two-way SRAM line cache is retained. The fixed PSRAM arena grows to 7.5 MiB, with 7832576 bytes allocated and 31744 bytes spare. A separate 512 KiB remains outside the arena for the SDK. No standard PSRAM malloc/pmalloc path is used. The existing all-block decode/CRC test is followed by stage 12, which checks CRCs of every resident cache page after the entire cache has been filled. This is intended to detect retention/address-alias errors across the larger PSRAM region on the actual board.

A real native host thread exercises the same render mailbox/backend against a synchronous, uncompressed, unaccelerated reference emulator. The 1200-frame and 6000-frame scripted runs pass full CPU/banked-register, exact cycle, bus/prefetch/latch, EWRAM/IWRAM and framebuffer comparisons. The 6000-frame run completes 959890 worker scanlines, with final frame hash 492ef131. Backing misses fall from 15367 in v19's same sequence to 540 in v20, including feature-scan reads. This is a cache-access comparison, not a measured Feather FPS improvement. The shorter run falls from 1413 misses to 522. Integrity/corruption, cache aliasing, graphics, input, EEPROM, arena and video checks also pass.

Flash output/GBAFeather-v20.uf2. The startup banner says BOOT v20, and the runtime first line shows 2core plus a C1 scanline counter. C1 should increase as game frames advance. Report FPS, Run/Draw/Copy ms/frame, decode/misses and whether C1 advances after settling at the same game screen. Run is core 0 elapsed emulation/finish time, Draw is core 1 render time, and Copy is presentation afterward. Run and Draw can overlap; do not add them. Timing includes any interrupts affecting the measured core. The display still counts completed emulated frames. The IRQ handler, CPU/PSRAM clocks and linker layout are unchanged from v19.

Hardware speedup, complete gameplay correctness and 60 FPS are unverified. The earlier post-menu stall was not definitively root-caused; PC/key/reads diagnostics and v19's graphics/input fixes remain. V19 is preserved as output/GBAFeather-v19.uf2 and source-preserved-v19.zip. The host test does not prove board boot stability or all possible rendering synchronization paths. Direct external Ppu mutations or raw framebuffer writes must be preceded by flush_render(); use Emulator.framebuffer() for a completed frame.

## Graphics/input baseline retained from v19

The user reports v18 reaches about 4.6 FPS but can appear stuck after the main screen: the LED and Run counter continue, FPS falls to about 2.8, PAD stays ON, Draw/decode/misses become zero, and sprites appear static or malformed. This is not a stopped firmware loop. Copy around 10 ms/frame is a duration, not a progress counter.

V19 fixes two independently reproduced PPU defects. Tile-cache tags encode only twelve generation bits; after 4096 invalidations the old code could accept stale decoded pixels. Both 4bpp and 8bpp caches now clear their keys before an encoded generation is reused. A 32x64 text background also selected block 2 instead of block 1 for the lower half; screen-block selection now uses the actual map width. The targeted tests failed on v18 and pass on v19.

Palette, tile and sprite-table changes now invalidate the appropriate caches. Palette writes reuse decoded indices and recolor them; VRAM writes invalidate tile decoding; OAM writes invalidate sprite scanline membership. Display-register writes no longer rebuild sprite membership, which depends only on OAM. Output tests cover independent palette, VRAM and OAM updates. This is intended to reduce Draw time without weakening invalidation correctness.

At 2–5 FPS a short real-world button press can begin and end between game input polls. Each UART press edge is now retained through the following complete guest frame, while held buttons remain held and disconnect/timeouts clear pending keys. The minimum press can therefore span roughly two presented frames. Brief repeated taps within that interval can merge. This changes input delivery, not guest timing or emulated CPU speed. The ESP32 firmware is unchanged.

Flash output/GBAFeather-v19.uf2. The last HDMI line now shows PC, key and reads. Cross should produce key bit 001; Options produces 008. Reads counts emulated KEYINPUT accesses. If it still stalls, report or photograph this line together with the game screen; whether reads changes and whether key responds distinguishes input delivery from an emulation loop. Run/Draw/Copy timing remains above it.

Native regression was extended from 1200 to 6000 frames with scripted A/Start/right input and periodic captures. Accelerated/compressed and unaccelerated/original-ROM paths retain matching full CPU, exact cycle, bus/prefetch/latches, RAM and framebuffer results. The scripted sequence reaches the file menu and later scenes but does not reproduce the exact board report or establish correct full gameplay. Its captured frame hashes did not change with the two PPU fixes, so these confirmed latent defects are not claimed as the proven cause of this particular stall. Hardware confirmation is still needed. The final suite also checks both cache formats, all four text map sizes, regional invalidation and input edge/release/disconnect/frame-wrap behavior.

The emulator still runs on core 0; multicore work was deferred to investigate the reported stall first. CPU clock, PSRAM timing, HDMI interrupt handler and linker layout are retained from v18. No frame skipping is enabled. Neither 60 FPS nor a fix for the exact board stall has been demonstrated. The v18 image and source-working-v18.zip remain available as a fallback.

## Performance baseline retained from v18

The user confirmed v17 boots and runs at about 3 FPS, with 6–10 ROM backing misses/second, 100–200 ms/second decoding, and HDMI reset count 2. Two resets are deliberately performed during startup; a count that keeps increasing would indicate runtime recovery. V17 and its source archive are preserved as the known-booting fallback.

V18 makes IRAM_ATTR place the annotated interpreter, memory bus, ROM reads, emulated hardware/scheduler and scanline rendering code in the linker's .time_critical SRAM sections. This marker was empty in the port, so v17 ran those functions from flash. The CPU condition table also moves to SRAM. Native builds leave the marker empty. The memory cost is about 35 KiB; no clock, PSRAM timing, IRQ handler, video resolution or guest timing change is introduced. Startup copies the new code into SRAM through the existing linker/startup mechanism. The compiled symbol addresses are checked for actual SRAM placement; defining a macro alone is not treated as validation.

Flash output/GBAFeather-v18.uf2. Report FPS plus Run/Draw/Copy ms/frame and the ROM decode line after settling on the same game screen. Run measures the whole emulator call, Draw is scanline rendering inside Run, and Copy is the RGB555-to-HDMI framebuffer conversion afterward. These are real elapsed durations including interrupt time. Draw is a subset of Run; do not add both. Timing uses the same Arduino micros() API already used by ROM decoding and does not alter the video interrupt handler. FPS counts complete emulated frames and is displayed to one decimal place. Every frame is rendered and copied, with no frame skipping. Hardware speed for v18 is unmeasured; 60 FPS has not been demonstrated.

## Working baseline retained from v17

The user reports v16 gives no HDMI signal throughout startup and the onboard LED stays off. This suggests a failure before the LED initialization in setup, but does not establish the cause. V16 is retained for investigation; do not use it as the current image.

V17 restores the v15 sketch and video interrupt wrapper, ROM timing implementation, and Emulator implementation, removing the v16 runtime/IRQ timing instrumentation. The sketch differs from v15 only in its version/recovery marker and enabling CPU RAM-wait acceleration after CPU reset. BoardStartup and the linker layout are identical to v15. The complete firmware was rebuilt with Arduino CLI --clean to avoid relying on cached objects. The user confirmed this version boots at about 3 FPS. V15 remains the known booting fallback at output/GBAFeather-v15.uf2.

The retained CPU optimization recognizes a five-instruction, read-only Thumb RAM polling loop, checks stable register, flag, prefetch/latch and loop timing state, and skips only whole iterations before the next scheduled hardware event. It does not change the guest clock or skip frames. Native tests compare full CPU/banked state, exact cycles, bus state, EWRAM, IWRAM and framebuffer for 1200 frames against the unaccelerated interpreter. The firmware-style sliced run skips about 60.5% of guest cycles. This is guest work removed, not a measured hardware speedup. Synthetic checks cover RAM flag wake-up, EWRAM/IWRAM, prefetch on/off, short execution boundaries and MMIO rejection. Full-game correctness and 60 FPS remain unproven.

Flash output/GBAFeather-v17.uf2 using BOOT + RESET, copy the UF2, wait for the bootloader drive to disappear, then allow startup checks to finish. Report whether BOOT v17 appears, whether the LED turns on/blinks, and the game FPS if it starts. The v17 HUD uses the v15 FPS, IRQ count and ROM decode display; v16's Core/PPU/IRQ timing and packet-age displays were removed. If there is still no signal, test the preserved v15 image to distinguish this new build from a flash/power/display problem.

## Performance and memory changes retained from v15

The user confirmed v14 boots the game on the Feather, at about 2–3 FPS. The working image remains `output/GBAFeather-v14.uf2`, and its sources were saved as `output/source-working-v14.zip` before performance changes.

V15 restores the HDMI library's 240 MHz CPU speed while retaining a 30 MHz PSRAM clock (divider 8). The QMI receive delay uses the maximum legal 3-bit value, 7; the exact sampling phase differs from v14 and still requires hardware confirmation. Maximum select/deselect timings are adjusted for 240 MHz. The memory divisor is applied with a dummy read/barrier before raising the CPU clock.

Static internal SRAM now holds GBA IWRAM (32 KiB), VRAM (96 KiB), palette/OAM (1 KiB each), and wait tables (64 bytes): 133184 bytes total, without runtime heap allocation. A separate 32 KiB two-way SRAM ROM cache uses 256-byte lines and reads from the existing CRC-checked 1 MiB PSRAM backing cache on misses. Direct ROM callback functions remove an extra virtual-dispatch layer. GBA EWRAM, PPU framebuffer, backing ROM cache, and HDMI recovery buffer remain in the tested fixed PSRAM arena (1541120 bytes used). The main stack stays 16 KiB.

Compilation uses `-O3`. The larger HSTX interrupt code occupies SCRATCH_X; the two unused SDK dummy stack reservations occupy SCRATCH_Y. This sketch does not launch core 1. Diagnostic redraws are throttled to once per second; every game frame is still rendered and presented, with no frame skipping. The bottom line reports live backing-cache misses/second and decode+CRC milliseconds/second, excluding startup verification/scan.

Native tests pass ROM boundaries, two-way conflicts, corruption rejection, direct callbacks, memory budgets, EEPROM, video-health checks, and CPU/framebuffer parity for 1200 frames. The SRAM ROM cache records 93467447 hits / 8411 misses (99.991% hits) over that language/title test sequence. This measures the access pattern on PC, not Feather FPS or full-game behavior. Flash `output/GBAFeather-v15.uf2` and report FPS plus the `ROM miss/s ... decode ... ms/s` line after several seconds at the same game screen. The user confirmed v15 boots but runs at about 1–2 FPS; use the v16 image above for the next performance test.

## Working startup path retained from v14

V13 reaches `36: CORE RETURNED TO SETUP` and the complete `Scan 8192 KiB`, then HDMI goes black while the regular 500 ms application heartbeat continues. V14 deliberately reinitializes HDMI after the scan's temporary buffer is released and again after save/BIOS allocation and emulator reset. These two planned resets use the existing PSRAM framebuffer backup and preserve emulator state. Signal interruptions during these resets are expected; allow the monitor time to relock after startup finishes. The underlying source of signal loss is still unconfirmed.

Automatic recovery now checks completed video DMA interrupt handlers as well as channel BUSY flags. A RAM-resident wrapper counts calls to the driver's original DMA_IRQ_2 handler; the original handler, priority, and video timing are retained. If the completion count does not advance for 100 ms, the display is restarted even when a channel remains BUSY. The probe is reinstalled after each driver reinitialization. Native tests cover timeout behavior, fresh interrupt progress, and counter/timer wrap; actual IRQ integration remains a board test.

During execution a once-per-second line shows `HDMI resets N | IRQ/s ...`, and CPU PC/cycle progress is shown while waiting for the first emulated frame. The existing video buffer, fixed PSRAM arena budget, 16 KiB stack, no-USB build, and LED patterns are retained. V14 is confirmed to start the game on hardware at about 2–3 FPS.

## Idle-DMA recovery retained from v13

The user reports the onboard LED still blinking with HDMI off in v12. That alone cannot distinguish application progress from the held recovery screen, because v12 used the same LED cadence for both. V13 uses two short pulses followed by a pause for a held recovery screen; the active application retains the 500 ms heartbeat. Explicit error screens keep the faster blink cadence.

Inspection of the installed HSTX driver shows that it chains DMA channels 0, 1 and 2 and does not restart a stopped chain. V13 samples those channels in the UART polling and text update paths. If all three remain idle for at least 20 ms over multiple observations, it preserves the RGB565 framebuffer in a preallocated PSRAM buffer, stops/reinitializes the existing HDMI driver, restores the framebuffer, and reports `HDMI DMA restarted: N`. It does not restart when any video DMA channel is observed active. This is a specific recovery attempt, not a confirmed diagnosis of the user's signal loss. A blocked restart is guarded by watchdog stage 10 during normal startup/game operation.

The 153600-byte recovery buffer brings the complete fixed arena budget to 1674304 bytes, leaving 422848 bytes within the tested 2 MiB reservation. Native tests cover transient/persistent DMA idle observations, timer wraparound, and the full new allocation budget. Actual HSTX restart and monitor relocking still require board testing. Flash `output/GBAFeather-v13.uf2` and report whether the display returns or which LED cadence remains if it stays off.

## USB-free stage transitions retained from v12

V11 reached constructor marker 34 with the expected 1520704-byte arena cursor and 16040 bytes of stack headroom at that point. This does not show stack or arena exhaustion. The next stage transition disabled the watchdog before USB logging and Arduino delay, leaving those operations without watchdog protection. V12 arms/updates the watchdog before any transition drawing or waiting, removes all sketch USB logging, and selects the core's `usbstack=nousb` build option. The emulator's unconditional BIOS debug message is also omitted on hardware. HDMI and the existing UART controller input remain enabled; the running firmware has no USB serial port. BOOT/RESET UF2 flashing still uses the ROM bootloader.

Marker 36 (`CORE RETURNED TO SETUP`) separates returning from the constructor from entering stage 3. Stage 3 still performs the normal feature scan with chunk progress callbacks. During ROM verification, scanning, and the game loop, the onboard LED follows a 500 ms heartbeat; it advances only when application work reaches the UART poll. Continuing blinks after lost video indicate application progress despite the lost HDMI signal. A held recovery/error screen also blinks, so report whether the monitor showed such a screen. V12 retains the 16 KiB stack and fixed bus-memory arena. This addresses a recovery/logging weakness; USB has not been established as the underlying cause. Flash `output/GBAFeather-v12.uf2` and report whether marker 36, stage 3, scan progress, or a recovery screen appears.

## Stack diagnostics retained from v11

V10 reaches marker 30 (VRAM allocation) and then loses HDMI without a reported recovery screen. A marker is written before the tracing callback returns, so this does not establish that the bump allocator itself is the failing operation. The v10 linker reserved only 2 KiB for the main stack. V11 reserves 16 KiB at 0x2007c000..0x20080000, reduces the heap limit accordingly, and keeps HSTX scratch RAM separate. The build script copies a local linker layout for this exact Feather 8 MiB flash/8 MiB PSRAM configuration; installed core files are unchanged. This tests possible stack exhaustion; it is not a confirmed diagnosis.

Constructor markers no longer print to USB or call Arduino delay/yield. They show the PSRAM arena cursor and current stack headroom. The arena remains 2 MiB, with the same allocations as v10. The boot recovery screen also recognizes a manual RESET while a diagnostic stage was pending. If video disappears and automatic recovery does not appear after 15 seconds, tap RESET once without holding BOOT. Report the last marker, arena value, stack value, or recovered stage. A second RESET leaves the held diagnostic screen and retries. Flash `output/GBAFeather-v11.uf2`.

## Fixed bus arena retained from v10

The v9 board test stops at marker 26, before the 32 KiB IWRAM allocation returns. The exact cause of that internal heap failure is still unknown. V10 places IWRAM, palette, OAM, and simulated wait-state tables in the fixed PSRAM arena too. EWRAM, VRAM, and the PPU framebuffer already used that arena. The Bus constructor now avoids the ordinary internal heap entirely; the core object and later standard-library vectors still use internal SRAM. This may reduce performance because emulated IWRAM now resides in external RAM.

The complete arena allocation sequence uses 1520704 of 2097152 bytes, leaving 576448 bytes. The native arena capacity test uses these exact allocation sizes and order. ROM/EEPROM/scan tests and 1200-frame CPU/framebuffer parity pass. Flash `output/GBAFeather-v10.uf2`. Marker 26 reads `26: IWRAM 32 KiB FIXED PSRAM`; successful progress reaches marker 27 and then the remaining startup steps. Hardware confirmation is pending.

## Constructor diagnostics retained from v9

The user reports v8 stopping at emulator construction with 347896 bytes free SRAM and an 83208-byte core object. V9 separates object storage allocation from placement construction and adds markers for framebuffer allocation/clearing, EWRAM/IWRAM/VRAM allocation/clearing, palette/OAM, wait tables, and constructor completion. Markers feed the watchdog between operations and persist the current operation for recovery. This is diagnostic instrumentation, not a confirmed fix. PC regression tests retain matching CPU/framebuffer results for 1200 frames. Flash `output/GBAFeather-v9.uf2` and report the last numbered constructor marker or recovered stage.

## Cartridge scan heartbeat retained from v8

The v7 hardware recovery screen reported stage 3 (cartridge scan), watchdog timeout, CFSR 00000000. The feature scan rereads the full ROM without a watchdog heartbeat, so its total duration can exceed the watchdog window even while work is progressing. V8 adds a callback after each completed 16 KiB scan chunk; the sketch feeds the watchdog, services UART, and displays progress every 256 KiB. A stuck chunk still times out. Native tests verify all 512 progress callbacks, cached scan behavior, EEPROM/no-RTC detection, and 1200 frames of CPU/framebuffer parity. Hardware confirmation is pending.

Stage 3 shows `Scan ... / 8192 KiB`.

## Startup diagnostics retained from v7

The board now completes ROM block verification with v6, but video fails afterward. Revision v7 retains the fixed arena and adds individual startup stages, free-SRAM/object-size reporting, and an 8-second watchdog around operations. A watchdog reboot restores HDMI and holds a screen showing the failed stage and CPU fault status. A timeout identifies an operation that did not return within the window; it does not by itself prove a hardware fault. Runtime slices also feed the watchdog, and first-frame presentation has a separate marker. This firmware compiles successfully but its recovery behavior still needs board testing.

If video disappears, wait for recovery and report the stage and fault status.

## Fixed PSRAM arena retained from v6

Hardware diagnostics established that the linker-reserved 2 MiB PSRAM region passes direct read/write tests at CPU 120 MHz / PSRAM 30 MHz, but the following `pmalloc()` call loses video (stage 5). This localizes the trigger to the standard external heap path; the precise underlying allocator failure has not been root-caused.

Revision v6 avoids that heap entirely. It reserves the same first 2 MiB via a NOLOAD section, uses an internal-SRAM allocation cursor with bounds/alignment checks, and never frees or allocates that storage through TLSF. The ROM cache is 1 MiB (64 slots), leaving room in the reserved region for GBA EWRAM, VRAM and framebuffer allocations. Allocation lifetime is until board reset; repeated emulator destruction/recreation is not supported. The slower CPU/PSRAM settings match the successful direct test. This prioritizes boot stability over frame rate.

The v6 workaround is compiled and regression-tested on PC; the user confirmed block verification completes on the Feather. Emulator startup has not yet succeeded on hardware.

The earlier stack fix is retained: tinf uses static internal RAM for decompression tables rather than a 1632-byte stack frame. Its measured frame is now 48 bytes. The decoder is single-owner/nonreentrant. The prior v2/v3/v4/v5 images and diagnostic folders are historical references, not the current game image. No installed Arduino core or Adafruit library files are modified.

## Flash the ready-made image

1. Keep the working HDMI adapter/cable setup.
2. Connect ESP32 GPIO25 to Feather RX/GPIO1, and GND to GND. Both use the existing 230400-baud protocol.
3. Connect the Feather to the PC. Hold BOOT, tap RESET, release BOOT.
4. Drag `output/GBAFeather-v20.uf2` onto the RP2350 drive.
5. The Feather checks all ROM blocks, then boots the replacement BIOS and game. Press PS to connect your controller.

The image already includes the ROM and the 8 MB PSRAM build configuration. You do not need to copy a separate ROM file to the board. The UF2 container is larger than the actual flashed payload because of its block framing. The existing controller HDMI sketch and standalone HDMI test remain available for recovery.

## Controls

| DualSense | GBA |
| --- | --- |
| Cross | A |
| Circle | B |
| D-pad or left stick | Directions |
| L1 / R1 | L / R |
| Options | Start |
| Create | Select |

For this European ROM, choose English with Cross at the language menu. The on-screen display shows controller connection, measured emulated FPS, CPU PC, cache misses and live timing. The user measured v15 at 1–2 FPS; v17 runs at about 3 FPS; v18 hardware performance is pending. The native PC tests are not an RP2350 performance benchmark.

## What was built

- The complete 8388608-byte ROM is preserved in 512 independent 16 KiB raw-DEFLATE blocks, falling back to raw storage if compression expands a block.
- Compressed payload: 3033341 bytes (about 2.89 MiB). Index and firmware consume additional flash. This leaves room within the Feather's 8 MiB flash.
- A direct-mapped 64-slot backing cache occupies 1 MiB of the reserved 2 MiB PSRAM arena. A miss decompresses and CRC-checks one block. A separate 32 KiB SRAM cache serves frequently accessed 256-byte ROM lines. The ROM is never fully expanded into PSRAM.
- GBA CPU, PPU, bus, timers, IRQ, DMA and cartridge logic come from the experimental `gba-embedded` core. The RP2350 port uses static SRAM for hot bus memory, fixed PSRAM for large buffers, and cached ROM reads.
- Boot uses the MIT-licensed Cult-of-GBA replacement BIOS supplied by SkyEmu, not a Nintendo BIOS dump. HLE-only boot was tried but did not boot this ROM correctly.
- The port adds serial EEPROM read/write support (6- and 14-bit addressing, DMA transactions), which this game needs. Read/write timing is simplified. Saves currently exist only in RAM and disappear on reset/power loss.
- HDMI uses the established 640x480/60 Hz HSTX timing with a 320x240 framebuffer. The 240x160 game image is centered, preserving its aspect ratio. Single buffering can show tearing.
- UART is serviced between 1024-cycle emulation slices; a 4096-byte FIFO absorbs short delays. Controls clear after 250 ms without valid packets. The interpreter runs on one CPU core. V15 runs the CPU at 240 MHz and PSRAM at 30 MHz while preserving the independent HSTX video PLL; CPU speed is reported at boot.

## Validation and limits

Verified on this PC:

- The fixed arena passes capacity, 16-byte alignment, and overflow/failure-atomicity tests.
- All 512 blocks decompress byte-for-byte to the original ROM; full SHA-256 is recorded in the generated manifest.
- The actual C++ cache passes 10000 random word/halfword reads, page-boundary reads, out-of-range reads, and corrupted-block rejection.
- EEPROM 6/14-bit address commands pass serial write/read round-trips.
- Compressed and uncompressed ROM providers produce matching CPU registers and framebuffer hashes for 1200 emulated frames, including A and Start input.
- Captured output reaches the language screen, then the Yoshi's Island title/menu. These are native-PC core tests, not a capture from your Feather.
- A startup hardware test verifies all compressed blocks again before emulation starts.

See `output/performance-v20-summary.json` for current build sizes. The first 2 MiB of PSRAM is a separate linker-reserved NOLOAD arena; internal SRAM holds hot memory, the ROM line cache, HDMI buffer, and remaining emulator state. The output UF2's framing and primary image addresses were checked against the 8 MiB flash limit. The separate architecture preamble is generated by the Arduino toolchain.

Limitations: no audio output, no persistent saves, no SD/menu for multiple ROMs, incomplete emulator compatibility, possible rendering/timing issues, and low measured RP2350 performance (v15: 1–2 FPS; v17: about 3 FPS; v18 pending). Reaching a title screen does not establish full gameplay correctness. The first board test should report the FPS and whether language selection / title / first level work.

## Rebuild

Run `powershell -ExecutionPolicy Bypass -File <workspace>/GBAPrototype/build.ps1`.

The script uses your installed Arduino-Pico 5.4.4 core, Adafruit libraries, board `rp2040:rp2040:adafruit_feather_rp2350_hstx:psram=8mb,usbstack=nousb`, C++20, `-O3`, port-specific include/feature flags, and the local Feather linker layout with a 16 KiB main stack. The linker layout contains resolved 8 MiB flash and 8 MiB PSRAM sizes and is specific to this board configuration. Opening the sketch and clicking Upload without these settings is not the supported build path; use the compiled UF2.

To change ROM or relocate the folder:

```
python GBAPrototype/tools/pack_rom.py "path/to/game.gba"
```

The packer regenerates the asset/index/assembly inclusion path and verifies lossless reconstruction. Rebuild afterward. Larger or less compressible ROMs may not fit flash. `build.ps1` produces the build image under `build/gba/`; the delivered copy is under `GBAPrototype/output/`.

Native tests use CMake/MSVC. `tests/prepare.py` expands GCC case ranges into equivalent case labels for MSVC only. The desktop Arduino stub uses ordinary heap memory in place of PSRAM. Tests use the same ported core, decoder, ROM-provider and EEPROM logic as the firmware. Expected desktop output is under `build/gba-host/`.

## Upstream provenance

- gba-embedded: https://github.com/bhoot1234567890/gba-embedded, commit `4ddb79e60f20979b8e438dfa00bf1c71a4a5a289`. Its README states no license is granted. This local experimental build must not be treated as a freely licensed or redistributable emulator release. Port changes are the RP2350 platform guards/allocations, portable header guards, and EEPROM hooks. A licensed core or upstream permission is needed before publishing a derivative release.
- tinf: https://github.com/jibsen/tinf, commit `57ffa1f1d5e3dde19011b2127bd26d01689b694b`. zlib license retained in `src/tinf/LICENSE` and source notices.
- Replacement BIOS: https://github.com/Cult-of-GBA/BIOS, supplied by https://github.com/skylersaleh/SkyEmu. The original attribution header and MIT license are retained beside the BIOS.

The user-supplied game, generated assets, and ROM-containing UF2 are excluded from Git. No game data is stored on the ESP32. No upload has been performed automatically.
