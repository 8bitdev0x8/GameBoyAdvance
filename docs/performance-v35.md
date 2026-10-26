# v35: direct Thumb ALU and constant-width instruction fetch

**Board failure:** the user reports CPU HARDFAULT with CFSR `00000000`.
Stage, PC/LR and HFSR are unknown. V35 is withdrawn as recommended firmware;
the native checks below did not predict target stability. Cause is unproven.
Use the confirmed working v34 fallback, or the narrower
[v36 isolation build](recovery-v36.md) to collect fault addresses.

## Target and scope

The user reports that v34 improved FPS, with Run 89 ms/frame and visible
CPU-dispatch/code-fetch hotspots. Numeric FPS, scene and duration remain
unspecified; the supplied total/split waits conflict. V34d corrects the HUD
rotation and is retained as the fallback. V35 targets CPU execution overhead
without changing the working four-job renderer queue or guest event timing.

No v35 board speedup or 30/60 FPS result has been measured.

## Execution changes

1. **Direct Thumb ALU path.** Opcodes `0x0000`–`0x3fff` execute in a small
   function forced inline into `cpu_run_until`. These are immediate shifts,
   register/immediate add/subtract, and immediate MOV/CMP/ADD/SUB. They touch
   low registers and flags, with no memory access, branch or internal-cycle
   side effects. All other Thumb instructions and all ARM instructions keep
   the original decoder. The legacy Thumb decoder remains available as the
   native reference; the new path is used only by the run loop, not `step`.
   Shift-by-zero/32 semantics, carry preservation, overflow and register
   alias ordering are explicitly preserved.
2. **Constant-width fetch.** CPU Thumb and ARM fetch now call `read_code16`
   and `read_code32`. Both instantiate the same preserved bus fast-path body
   with a compile-time access width. The compiler can remove width checks,
   alignment alternatives and load-size branches. Ordinary `read_code`
   dispatches to them for the existing test/API callers. Byte, DMA, special,
   bounds and failed-prefetch cases retain ordinary bus reads. Prefetch
   transitions, open-bus/latches, waitstates and hints retain their logic.
3. **ROM address-latch reader in SRAM.** `Bus::read_gamepak_rom` gets SRAM
   placement without changing its body. It previously executed in flash
   while called by the SRAM bus reader. This is one bounded placement
   change, not the unvalidated v29 relocation bundle.

The selected Sonic ROM, original BIOS, BIOS/HLE policy, cache organization,
CPU/PSRAM clocks (240/30 MHz), timers, IRQ behavior, DMA, APU event cadence,
rendering, HDMI driver, UART and linker are unchanged. No game-frame or
scanline skipping is added.

The CPU profiler labels width-specific fetch as `Fetch Thumb16` and
`Fetch ARM32`. Direct arithmetic now resides inside `CPU dispatch`, so a
larger dispatch sample share and smaller `CPU Thumb` share can reflect
moving work between functions rather than a slowdown. Compare sustained
FPS/Run and the same scene, not percentages alone.

## Rejected experiment

The existing verified polling-loop accelerator leaves a 16-guest-cycle tail
before its event deadline. An isolated experiment removed that tail to skip
additional stable iterations. It passed the RAM-wait fixture but failed
VBlank CPU/banked/IRQ parity. It was rejected. V35 retains the original tail,
stability proof, skip arithmetic and timer-service cadence. The rejected
source/binary/log are kept separately under the private build directory.

## Validation

The focused audit compares every encoding in `0x0000`–`0x3fff`, all 16
initial NZCV combinations and varied edge operand patterns: **262144 cases**.
The new and legacy decoders match full CPU state, exact cycles and bus
prefetch/latches, including low-register aliases and shift amounts. This
does not claim every possible 32-bit operand combination is exhaustive.

Existing audits also pass 30000 mixed instruction reads, 600120 BIOS scalar
reads plus 120000 protected/code/data/DMA BIOS reads, 263168 instruction-window
cases plus 60000 mixed bus reads, 45003 video writes, 24030 register writes,
600 DMA cases, 8160 Thumb multiple-register cases, and 60000 independent
timer-reference operations.

The 6000-frame Sonic regression disables the new ALU path on the conservative
original-ROM reference and enables it on the compressed/optimized instance.
Every frame matches CPU/banked registers/flags, exact cycles, bus state,
EWRAM/IWRAM and every RGB565 pixel/protected margin. Display target replacement
at frame 3000 and split-wait accounting pass. All 20 checkpoints match v34:
PC `080bc248`, frame hash `5ba30470`, 1685293130 cycles, 11737 input reads,
953015 rendered scanlines. **42879024** instructions use the new ALU path;
the fetch/cache counts are unchanged. Native counters and the ALU reference
toggle are absent from the ARM class layout.

Three-repeat native timing trials (1200 warmup + 2400 measured frames) show
no substantial improvement: baseline compute median 1019.768 ms versus
1004.966 ms for the combined fetch/inline-ALU variant; threaded medians
1654.626 and 1682.830 ms respectively. Windows scheduling/cache behavior and
different compilers prevent translating these into RP2350 FPS. The early
ALU-only trial also had host audit-counter/call overhead and is not a clean
board-performance prediction. SRAM placement benefits cannot be established
by desktop RAM timings. These results support an experimental board trial,
not a claim of a measured large speedup.

## ARM artifact and memory

The final ARM assembly calls the width-specific fetch entry points directly,
and has no standalone ALU helper: it is inlined into the SRAM run loop.
The ROM address-latch reader is also confirmed in SRAM. Static stack frames
are 72 bytes for `cpu_run_until` and each fetch specialization, 80 bytes for
the general Thumb decoder, and 24 bytes for the relocated reader. These are
individual frames, not a complete call-tree stack bound.

The build reports 232296 static SRAM bytes (+1504 vs v34d) and 4980172
program bytes. Main/core-1 stack reservations, emulator/cache object sizes
and the fixed PSRAM budget are preserved. The PC map contains 825 functions
and is verified against the final ELF. Full UF2/vector/ROM/BIOS/SRAM/source
validation and checksums are in `output/performance-v35-summary.json`.

## Board trial

Flash `output/GBAFeather-Sonic-v35.uf2` with BOOT + RESET. Banner:
`BOOT v35 - SONIC / CPU FAST PATH`. Compare the same Sonic screen and
controller state for at least 20 seconds. Capture numeric FPS and Run/Draw/
Wait, then the matching-number Qfull/Barrier row. IFetch hit/fills and the
new fetch labels help explain the remaining costs.

The manual fallback is `output/GBAFeather-Sonic-v34d.uf2`, SHA256
`d0f6bb0177d894fa6af4266f45fc760e107cb6263f2c116dd98ed8c9d8cc2817`.
The confirmed working v34 and earlier images remain intact. There is no
automatic dual-image fallback. No modified core, ROM, original BIOS or
firmware is published.
