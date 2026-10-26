# v34: four-job renderer queue and one-pass schedule refresh

## Board evidence and target

The user's v33 profile reports submit_hook 22%, Bus code fetch 12%, CPU
dispatch 8%, and Wait 24 ms/frame. FPS/Run/Draw, scene/duration, total sample
N and IFetch hit/fills are unspecified. These percentages do not establish
a speed regression or improvement. Submit includes the inlined WFE wait,
and Wait is included in Run.

V34 tests the safe pipeline changes from
[DEEP-PERFORMANCE-ANALYSIS.md](DEEP-PERFORMANCE-ANALYSIS.md). The four-entry
queue plus scheduler change improved the desktop threaded test median by
about 6.5%, with host run-to-run variation. This is not a board FPS forecast.
The user subsequently confirmed improved FPS, PAD ON, VBskip 20%, Run 89,
Draw 2, Copy 0 and Wait 0 ms/frame; Qfull 16 and Barrier 13 ms/frame, depth
four; Bus code fetch 16% and CPU dispatch 17%. Numeric FPS is unspecified.
The total/split waits conflict if from one snapshot. A confirmed four/five
phase HUD ownership bug is corrected in
[DIAGNOSTICS-v34d.md](DIAGNOSTICS-v34d.md); its role in this particular
reading remains unproven. No controlled numerical speedup is established.

## Changes

The old worker drained its single pending job before every new line. V34
queues up to four jobs in a bounded single-producer/single-consumer ring.
The producer waits only when that ring is full or an existing video-state
barrier requires completion. The consumer checks dirty state, renders dirty
lines, and completes clean jobs without copying a stale row buffer.

Jobs are published with release/acquire synchronization. A slot becomes
reusable only after its PPU reads, output stores and elapsed-time update
finish. All changed video-memory/register writes still drain the queue;
target detach/rebind, reset and frame completion do too. There is one
renderer and one shared PPU workspace. No frame or scanline rendering is
intentionally dropped.

Schedule refresh now assigns the six existing deadlines and computes their
minimum once, replacing six separate setter calls. CPU instruction semantics,
guest event deadlines, DMA/timers/PPU/APU behavior, BIOS, ROM and instruction
cache, clock settings, UART, linker and installed HDMI driver are unchanged.
The failed silent-audio event-coalescing experiment is excluded.

## New wait diagnostics

The diagnostic row rotates through five five-second phases: HDMI,
PC sample status, third PC hotspot, IFetch and the new queue line:

`Qfull <n> Barrier <n> ms/f | depth 4`

* Qfull is time core 0 waits for a free job slot during submission.
* Barrier is time spent explicitly draining jobs before shared-state or
  lifetime changes, or frame completion.
* Their unrounded counters sum to the existing total Wait counter modulo
  timer wrap; separately rounded displayed values can differ by 1 ms/frame.

These are core-0 elapsed waits, not core-1 utilization. A deep queue can move
waits from Qfull to Barrier without reducing total frame time. Draw still
overlaps Run, and Wait remains part of Run; do not add all HUD values.

## Validation and memory

The 6000-frame native regression compares the threaded compressed/optimized
build to the conservative synchronous original-ROM reference. Full CPU and
banked registers/flags, exact cycles, bus/prefetch/latches, EWRAM/IWRAM and
every RGB565 pixel/protected margin match. Display storage replacement at
frame 3000 passes. All 20 checkpoints match v33: final PC `080bc248`, hash
`5ba30470`, 1685293130 guest cycles, 11737 input reads, and 953015 rendered
scanlines. Split wait counters sum exactly to the total.

Focused tests exercise dirty/clean/duplicate jobs, forced blank, ring wrap
and pending-job detach/rebind. Existing instruction, BIOS, bus, video-write,
DMA and independent timer checks pass. This is regression evidence, not a
physical-GBA or complete-game accuracy proof.

The ARM build reports 230780 static SRAM bytes (+424 vs v33) and 4980204
program bytes. Main/core-1 stack reservations and fixed PSRAM budget are
unchanged. The final profile map has 824 functions and is checked against
the exact final ELF. Full artifact/memory validation is recorded in
`output/performance-v34-summary.json`.

## Test and fallback

Flash `output/GBAFeather-Sonic-v34.uf2` with BOOT + RESET. Banner:
`BOOT v34 - SONIC / RENDER QUEUE`. Keep the same slow Sonic screen for at
least 20 seconds and record FPS, Run/Draw/Wait, Qfull/Barrier and IFetch
hit/fills. Compare sustained frame time, not changes in sample percentages
alone. No 30/60 FPS result is claimed.

The preserved manual-reflash fallback is `output/GBAFeather-Sonic-v33.uf2`,
SHA256 `ccbceddca8f08cee038d9d05172e477ac696d1f1c924a7e972e4463a56cf1a2b`.
V32/v31/v30 remain available. There is no automatic dual-image fallback.
No modified core, ROM, original BIOS or UF2 is published to GitHub.
