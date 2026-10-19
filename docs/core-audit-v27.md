# Core audit and performance work, v27

Two Thumb multiple-transfer correctness defects were reproduced and fixed. V27 also reduces instruction-fetch and timer-service overhead while retaining v26's direct-render pipeline, clocks, HDMI driver, IRQ wrapper and linker. The user subsequently confirmed v27 at approximately 7 FPS average, Run 121, Draw 33, Copy 0, Wait 32 ms/frame and VBskip 10%; scene/duration were not specified. Follow-up work and proposed real-board profiling are in [performance-v28.md](performance-v28.md).

The latest explicitly identified v26 hardware report is approximately **6-7 FPS**, **VBskip 6-12%**, **Draw 49 ms/frame**, **Copy 0**, **Wait 26 ms/frame**, ROM misses 0 and decode 0-19 ms/s. The game scene and Run timing were not supplied. Draw includes conversion/presentation, Wait is included in Run, and Draw can overlap Run. This is the comparison baseline, not a controlled same-scene benchmark.

## Reproduced correctness issues

| Issue | Before | Fix / verification |
| --- | --- | --- |
| Thumb LDMIA with its base in the register list | Loaded base overwritten by address writeback; a single-register `LDMIA r0!, {r0}` returned the advanced address rather than loaded data | Retain the loaded base value; exhaustive nonempty-list/base-register cases |
| Thumb STMIA with its base later in the register list | Stored the old base rather than ARM7's updated base value | Store the final base for that case, retaining the old base when it is first |

The expectations follow the ARMv4 Thumb behavior described in [GBATEK's multiple-transfer section (mirror)](https://rust-console.github.io/gbatek-gbaonly/). No ROM patch is involved. Before-fix evidence: [STMIA failure](evidence/core-v26-before-fix.log) and [LDMIA failure](evidence/core-v26-ldm-before-fix.log).

The independent instruction test covers **8160 cases**: all 255 nonempty lists, eight base registers, load/store, EWRAM/IWRAM, base first/later/absent, unlisted registers and flags. Both old-code defects were observed before the change; the corrected implementation passes. The implementation visits selected registers instead of scanning all eight positions and calculates a population count only for the exceptional store-base case.

In the scripted 6000-frame Sonic sequence, neither corrected base-in-list case was executed. These are confirmed accuracy fixes, but they are not an established cause of Sonic's earlier stalls or its low FPS.

## Performance changes

### Instruction fetch no longer always takes the general data-access path

The old fetch path entered the same broad bus dispatch used for BIOS, RAM, MMIO, video memory, ROM, SRAM and peripheral reads. V27 handles common IWRAM/EWRAM instruction reads and already-active GamePak prefetch reads through a smaller path. It preserves the value, guest cycles, prefetch state, bus latches, access ordering and ROM-provider behavior.

BIOS/MMIO, EEPROM, data/DMA accesses, unsupported widths, ROM bounds and uncommon prefetch states fall back to the ordinary bus reader. ROM RTC GPIO still goes through the cartridge reader. Randomized mixed reads and explicit 32/128 KiB boundary, partial-ROM and RTC cases compare the new path with ordinary reads. The original fetch path remains selectable for native comparisons.

The fetch API returns just value and cycles packed into a scalar 64-bit return. On the target ARM ABI that avoids the caller-provided temporary result-structure buffer used by the general bus API. The guest instruction fetch had never consumed the general reader's data-access flags; all guest-visible bus-state updates remain.

The native Sonic sequence uses the fast path for **89126473 of 109063662 instruction reads, about 81.7%**. This measures coverage, not a speedup percentage or hardware CPU utilization. Slow ROM-cache fills and decompression are unchanged.

### Timer deadlines are cached

Previously the core repeatedly searched four timer channels for pending writes and overflows, even when no event could yet be due. The scheduler also recomputed the earliest timer event repeatedly. V27 maintains the earliest deadline after reset, timer register writes and actual event processing, including cascade and delayed reload/control operations.

An inline public guard checks that deadline before entering the larger timer-event handler. Assembly inspection initially showed that a check inside the handler still paid its large register/stack prologue; the guard was moved outside it. A separately compiled preserved v26 timer implementation checks **60000 randomized operations**, comparing complete public channel fields, register reads, split-cycle reads, deadlines, IRQ scheduling and FIFO requests.

### Tiny hot helpers stay inline

The two APU FIFO-request flag checks now inline with identical behavior, avoiding repeated flash calls during timer servicing. Prefetch bookkeeping is forced inline under GCC: assembly review found the compiler had outlined a shared helper back into flash after the new fetch path was added. This change avoids that repeated SRAM-to-flash trip.

These changes reduce host work. They do not reduce guest cycles, omit emulated hardware events, change clocks or skip rendered frames.

## Whole-emulator regression

The 6000-frame test compares compressed-ROM/fast-fetch/accelerated/silent-APU/threaded direct-video execution with original-ROM/ordinary-fetch/unaccelerated/synthesis-enabled/synchronous framebuffer execution. It checks CPU/banked state and flags, exact cycles, bus/prefetch/latches, EWRAM/IWRAM and every converted display pixel with protected margins. It also tests replacement of the display allocation at frame 3000.

All 20 PC/hash/input-read checkpoints remain identical to v26. Final PC is `080bcca6`, framebuffer hash `5ba30470`, guest cycles 1685292894, input reads 11861, VBlank cycles bypassed 347342144 and worker jobs 958816. The native counters showing fast-fetch coverage and corrected-case use are absent from the hardware build.

These comparisons check regressions within this experimental emulator, not accuracy against a physical GBA. The scripted game run reaches language/name entry rather than demonstrating a complete playthrough. The subsequent v27 hardware report confirms startup and approximately 7 FPS, without establishing a controlled same-scene speedup over v26.

## Remaining issues and limits

- Empty-register-list ARM/Thumb transfer behavior is still not correctly implemented for ARM7. This is a flagged accuracy gap, outside the nonempty-list fix above.
- ARM block-transfer PC/base edge cases need further instruction-level validation. This was not an exhaustive CPU conformance audit.
- The backend is still an interpreter; a large number of guest instructions, bus operations and scheduling decisions remain.
- Renderer time and synchronization remain relevant: the earlier v26 Draw 49 / Wait 26 ms report is above the budget needed for full-speed rendering, even with Copy removed.
- HSTX still uses the original per-line DMA IRQ handler. Its actual CPU-time share has not been measured; IRQ count alone cannot establish it.
- No claim is made that these changes reach 30 or 60 FPS. The framebuffer remains single-buffered, sound output and persistent saves are absent, and complete gameplay correctness is unproven.

## Reproduce the local audit

The matching modified core and v26 source archive are deliberately private because upstream grants no redistribution licence. These commands document the local workflow and do not make the public support-only repository a complete emulator build.

```powershell
python GBAPrototype/tools/prepare_timer_reference.py GBAPrototype/output/source-sonic-v26.zip --out build/timers-v26-reference
python GBAPrototype/tests/prepare.py
cmake -S GBAPrototype/tests -B build/gba-host "-DGBA_TIMER_REFERENCE_DIR=$((Resolve-Path build/timers-v26-reference).Path)"
cmake --build build/gba-host --config Release --parallel 1
build/gba-host/Release/gba_core_audit.exe
```

The 6000-frame test additionally uses `GBA_TEST_THREADED=1`, `GBA_TEST_SLICED=1`, `GBA_TEST_AUTO_INPUT=1`, `GBA_TEST_MUTE=1`, `GBA_TEST_DIRECT_VIDEO=1` and `GBA_TEST_FRAMES=6000`, with locally supplied ROM and generated page data. V27 and v28 are subsequently confirmed working; this section describes the v27 audit workflow. Do not publish core source archives or firmware containing the game.
