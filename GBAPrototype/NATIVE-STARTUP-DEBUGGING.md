# Native startup debugging: verified upload, no USB

Recorded 21 October 2026, Europe/Dublin. This is a startup investigation, not a
native performance result. Game-derived code, firmware, ROM and BIOS remain
private.

## What happened

The first complete native-runtime image uploaded and verified at 00:58:22.
The Feather then disappeared from runtime USB. No `NRT_INFO`, recovery record,
or frame record was captured. The headless image deliberately omitted the
HSTX driver, so no HDMI signal was expected even if startup had succeeded.
Absence of video and absence of USB were separate observations.

Root stopped its capture worker and made one V52 recovery attempt. Neither
runtime USB nor the serial-pinned BOOTSEL interface was available. Hardware
retries stopped; manual BOOT/RESET recovery is required before the next upload.
The raw upload, empty capture and failed recovery are retained privately under
`logs/perf-lab/native-runtime-board-01/`.

The original capture timeout was too generous for a board that never announces
startup. The runner now distinguishes a 90-second startup deadline from the
longer game replay deadline. This change does not retroactively recover a
missing fault record.

## How the failure was narrowed down

The working image-loader probe had already expanded and verified the entire
4,754,296-byte native image in physical PSRAM. A separate small-function probe
had executed instructions from that PSRAM. These establish useful capabilities;
they do not establish that the complete runtime boots or performs well.

The audit compared the exact failed ELF with the successful loader and V52:

- The reset vector, stack location, data-copy bounds and BSS bounds were checked.
  The large PSRAM reservation is `NOLOAD`; CRT startup does not clear it as BSS.
- The SDK initialization arrays and C++ constructors were compared. No new
  Rust or ROM constructor explained the failure.
- An initial suspicion about even disassembly addresses was rejected: actual
  ELF Thumb function symbols were correct. The inspected memory routines also
  showed no recursive `memset` implementation.
- The PSRAM initialization call graph was followed through veneers to its
  actual linked division implementation, rather than assuming a familiar
  symbol name still referred to the SDK implementation.

## The concrete residency regression

Arduino-Pico's `psram_init` executes from SRAM. It enables QMI direct mode,
configures the PSRAM, and computes its clock period using a signed 64-bit
division before leaving direct mode. Flash instruction fetches are unsafe
during this interval.

In the failed image, the division call went through an SRAM veneer into the
Rust archive's Flash-resident implementation:

| Routine in the failed ELF | Address range | Placement |
| --- | --- | --- |
| `__aeabi_ldivmod` | `0x1001077c`–`0x10010790` | Flash |
| `__divmoddi4` | `0x10011016`–`0x1001108a` | Flash |
| Specialized unsigned division helper | `0x1001108a`–`0x100113ec` | Flash |

The successful loader uses the SDK/libgcc implementation in SRAM. The linker
script deliberately excludes libgcc text and constants from its Flash collector
and includes them in copied SRAM data. The new Rust archive was outside that
archive-specific rule, and its runtime symbol replaced the expected provider.
A successful link therefore did not imply safe execution during PSRAM startup.

This is a definite code-placement hazard and a likely explanation for the
USB-less boot. A board retest is still required to establish that correcting it
resolves the observed failure. No guest instruction execution is inferred from
the absence of USB.

## The new HDMI build

The separate `build/native-runtime-hdmi-board/` clone restores SDK/libgcc
division ownership before linking the unchanged Rust archive. Its final ELF
must put the complete reachable division helper and constant-data closure in
SRAM; moving only the entry wrapper would leave the same problem underneath.
The audit also checks `clock_get_hz`, which the SDK calls inside the same
direct-mode interval, and its frequency table.

The build adds the existing legacy HSTX driver on core 1. Core 0 submits each
completed 240×160 RGB555 frame through a bounded two-row queue and waits until
the borrowed source pixels are consumed. Core 1 converts into the SRAM scanout
buffer and owns all display and diagnostic writes. Guest CPU, PPU, APU and
hardware timing work remain enabled; frame-copy and display backpressure are
included in the measured window.

The screen identifies the image as **NATIVE HDMI TEST 1**, shows startup stages,
and holds the final frame with a PASS/FAIL and measured FPS. The workload is the
fixed scripted 5,180-frame replay, not yet a general controller-driven native
frontend. An early Arduino hook records failures before USB initialization;
it runs after PSRAM preinitialization and cannot replace the linker correction.

Compile, layout and offline correctness checks must be reported separately from
physical startup, HDMI stability, native replay correctness and FPS. Until the
board replay passes, the best completed matched HDMI measurement remains the
legacy typed-ARM trial at 7.62210 FPS.

The completed Native HDMI v1 build passes the Feather compile, asset/layout
checks and an independent audit of the entire identified QMI call closure.
The root release verification checks all 77 manifest entries and the unchanged
Rust runtime archive. Its UF2 is private at
`output/native-hdmi-v1/GBAFeather-Native-HDMI-v1.uf2`, SHA256
`d645330d5f0f9144fd12c7b70f77c3955b105600c05168b5e75a77889187a6a0`.
The Flash span is 6,580,480 bytes of 8,388,608 bytes; the larger UF2 file also
contains block headers and transport padding.

The Feather was subsequently detected in serial-pinned ROM BOOTSEL mode.
One new upload/capture attempt is running under the hardware lock in
`logs/perf-lab/native-runtime-hdmi-v1-01/`. Its outcome must be read from that
capture; the completed offline checks alone are not a board success or FPS gain.

The new upload subsequently verified and USB enumeration succeeded. Captured
`NRT_HDMI state=1` reports core1 scanout, 240 MHz CPU, 126 MHz HSTX and an intact
core1 stack canary. The full native image/tables verify in 9,371 ms. Runtime
construction completes in 32,461 ms, using 617,717 charged PSRAM bytes plus
the separate 2 MiB ROM backing; budget/backend/provider failure counts are zero.
The strict native workload reaches completed frames 1 through 10 on the real
board. This confirms progress beyond the previous startup failure in the new
composition. It is not an isolated causal A/B of the linker fix: HDMI, its clock
startup and recovery diagnostics also changed. Monitor appearance and the full
5,180-frame endpoint/FPS outcome remain separate checks.

The user then confirmed that the monitor retains a signal but the entire
picture is black. This rejects Native HDMI v1 as a usable display result,
despite USB progress past frame 3,000 and active scanout interrupts. The monitor
observation is retained beside the capture. Core1 initialization/IRQ ownership
is a new platform difference from working V52, not a proven cause. A separate
v2 control restores initialization and DMA_IRQ2 to core0 and adds startup RGB
bars and framebuffer/FIFO/DMA diagnostics. The default single-buffer argument
was checked and ruled out: v1's explicit `false` matches V52's default.

Native HDMI v1 subsequently completes the physical replay. Both measured
endpoints (frames 5,000 and 5,180) and all required native/service/cycle counters
match its frozen runtime oracle. Interpreter-path and return-mismatch counters
remain zero. The 180-frame window takes 45,027,177 microseconds: **3.997586 FPS**.
This is retained as an experimental execution result, not a usable HDMI result
or a performance gain. The monitor observation remains a failure.

The final IRQ count is 69,165,332. Dividing by the entire 854.286844-second
upload-to-terminal interval gives a conservative lower bound of **80,963 IRQ/s**,
well above the approximately 31,500 scanline interrupts expected for this mode.
The counter starts after upload, so the actual interval average is higher.
This establishes abnormal scanout activity; it does not by itself identify the
precise DMA phase or command-stream failure. The existing V52 handler assumes
the expected channel completed, while V52's additional rate/stall monitor can
reset malformed output. V1 did not include that recovery monitor.

The V52 fallback is verified by its runtime map fingerprint. Its recovered
telemetry reports 32,618 IRQ/s after six startup resets. The separate v2 control
is queued only after that fallback completes, with source-pixel snapshots and
DMA/FIFO diagnostics to distinguish a drawing error from malformed scanout.

## 25 October: Test 3 result

The unfinished third candidate was audited, documented and frozen. All eleven
retained DVI timing records are in SRAM and match Test 2 byte-for-byte. QMI
helper closure, image/assets, ABI, memory budgets and UF2 load ranges pass.
This does not establish physical display success.

Physical test `native-runtime-hdmi-v3-2026-10-25_14-12-36` boots and loads its
native image successfully. Scanout averages 31,472 IRQ/s through image loading,
then 84,066 IRQ/s during the 54,720-ms ROM metadata scan. At that point the
framebuffer still has 33,600 non-black test-pattern pixels and 2,501 HUD pixels.
The rate failure therefore precedes guest instruction execution. No CPU/ROM
provider allocation errors are reported. Native execution reaches frame 10
while final diagnostics are drained; the runner then stops the trial early.

The SRAM timing-data relocation did not prevent the observed scanout failure.
No measured gameplay window or FPS improvement is claimed. A single V52
restoration succeeds and runtime map fingerprint 2447646605 is verified; the
restored telemetry reports 33,101 IRQ/s after five startup recovery resets.
The full raw USB, timestamped events, failure decision and fallback proof are
retained in that capture folder. Current follow-up work is described in the
[reference-guided optimization plan](REFERENCE-GUIDED-OPTIMIZATION.md).
