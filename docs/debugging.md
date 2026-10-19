# How the project was debugged

The hardest symptom was a monitor saying "No signal." That describes the display link, not the underlying fault. It can accompany a stopped CPU, a blocked allocation, a watchdog reset, a broken video DMA chain or a running application whose video output has stopped. The investigation gradually added ways to distinguish those cases without relying on USB logging.

## Prove the chain in small pieces

The project established controller input and video separately before combining them with an emulator:

1. Read a complete DualSense report on ESP32.
2. Send framed reports over UART and validate them on Feather.
3. Display a standalone HSTX test pattern.
4. Display controller values through HDMI.
5. Exercise PSRAM without an emulator.
6. Verify compressed ROM blocks and cartridge scanning.
7. Construct/reset the emulator, then wait for its first frame.

The published sketches preserve those isolation steps. If the controller viewer works but the GBA frontend does not, the whole Bluetooth-to-display chain does not need to be reinvented. Conversely, a software compile succeeding says nothing about ribbon orientation or a monitor accepting the signal.

The early browser dashboard's SerialPort open failure belonged to a different layer: a host serial connection, potentially competing with Arduino Serial Monitor. That dashboard is no longer in the runtime architecture.

## Display the next operation before doing it

Boot screens show a numbered stage before entering a risky operation. Main stages include ROM verification, construction, cartridge scanning, save setup, BIOS loading, reset, waiting for a first frame, presentation and normal execution. Nested constructor markers distinguish object storage, framebuffer, EWRAM, IWRAM, palette/OAM, VRAM and wait-table allocation/clearing.

The stage survives a failed operation through watchdog scratch registers. A version marker avoids interpreting an older build's scratch data as the current build's failure. Stage 36 distinguishes "constructor returned" from the following setup work.

The last visible marker only bounds the investigation. If stage 30 appears before blackout, code in the stage drawing path, the next operation or later work before the next marker could be involved. It does not prove the VRAM allocation itself failed.

## Use watchdog and fault information carefully

The watchdog is armed before transitions and refreshed during bounded chunks of ROM verification/scanning and ordinary polling. A HardFault hook records a fault marker and Cortex-M CFSR, then leaves the watchdog to reset the board. On reboot the frontend can hold a recovery display instead of immediately hiding the failure with another boot attempt. Manual reset can preserve a pending-stage report too.

One concrete report was stage 3, cartridge scan, watchdog operation did not return, CFSR `00000000`. This established a timeout observation, not a CPU fault diagnosis. Progress callbacks were added to service UART, refresh the watchdog and show scanned bytes per ROM chunk. Cached cartridge-feature results avoid redundant full scans.

Some failures never produced a recovery screen. Recovery itself depends on startup and video functioning after reset. Absence of a report cannot be taken as proof that no fault occurred.

## Separate external memory from the allocator

An early HDMI/PSRAM test called `pmalloc()` before proving mapped memory worked. If it stalled, possible causes included QMI timing, PSRAM access, allocator initialization and corruption of allocator metadata.

The direct test instead reserved a 2 MiB NOLOAD PSRAM array through the linker. It performed one-word access, two-word read/write and full-pattern write/verify before invoking `pmalloc(4096)` in the remaining heap. The user reported the direct test passing and failure at the later allocation stage. This made an allocator-free path useful, but did not identify an exact TLSF defect.

The workaround was a fixed-lifetime arena with its cursor in internal SRAM, explicit capacity checks and 16-byte alignment. Later tests filled the much larger ROM backing cache and checked resident CRCs to exercise retention/addressing across the intended region. A reported PSRAM capacity is not a substitute for memory integrity tests.

CPU and PSRAM clocks were treated separately from HSTX. Conservative 120 MHz CPU / 30 MHz PSRAM diagnostics preceded the 240 MHz CPU / 30 MHz PSRAM emulator configuration. The installed video clock arrangement was preserved rather than assuming every CPU clock change was harmless to output.

## Check stacks and the actual linked image

The decompressor initially had a 1632-byte stack frame. Moving tables to static SRAM reduced its measured frame to 48 bytes, but also made it nonreentrant. This was one stack-pressure reduction, not proof of the original blackout's cause.

The main stack reservation was increased from 2 KiB to 16 KiB using a local linker script. The worker later received an explicit 8 KiB stack and canary. Build validation checked the vector's initial SP, stack symbols, heap limit, PSRAM range and initialized RAM load segments. Compiler `.su` reports described individual function frames; they were not claimed as a measured maximum whole-call-stack depth.

When `IRAM_ATTR` turned out to be empty in the initial compatibility shim, annotated hot functions still ran from flash. The corrected macro used `.time_critical` sections, and `nm` verified their final addresses in SRAM. A macro name alone did not demonstrate placement. ELF-to-UF2 comparisons also checked that initialized SRAM bytes were present in the flashed payload.

## Distinguish a live application from a live video pipeline

The normal application's LED toggles at roughly half-second intervals when work reaches UART polling. Recovery uses two short pulses with a pause; explicit errors blink faster. Early identical LED patterns made recovery look like progress, so the patterns were separated.

The first video monitor observed whether DMA channels 0-2 stayed idle for at least 20 ms. A later monitor counted *completed* calls to the original DMA IRQ handler and treated 100 ms without progress as a stall. Both have wraparound tests. A restart saves the framebuffer to preallocated PSRAM, ends/restarts HSTX, reinstalls the wrapper and restores pixels, with a cooldown to avoid repeated immediate resets.

V14 also intentionally restarted HDMI after scan and emulator reset. A reset count of two can therefore be normal startup, while an increasing runtime count calls for investigation. These recoveries got a game on screen; they did not prove a root cause for every lost-signal event.

The original IRQ handler is retained because it performs per-line video work. Replacing it with a counter or acknowledgment would break output. The observed 31 kHz IRQ rate was not converted into a fictitious CPU-time percentage.

## Investigate apparent game stalls with more than one counter

The HUD gained PC, delivered key bits, input-read counts, ROM misses/decode and Run/Draw/Copy timings. Each answers a different question:

| Observation | What it supports / does not prove |
| --- | --- |
| PAD ON | Valid connected-controller reports arrived; not proof the game consumed the intended button |
| PC changes | Instructions execute; not proof the game escapes a busy-wait |
| KEYINPUT read count changes | Guest reads the input register; not proof menu logic accepts it |
| C1 job count changes | Renderer jobs finish; not proof pixels or game state change |
| Draw becomes zero | Little/no measured drawing in that interval; could be an unchanged scene or another issue |
| ROM decode becomes zero | Requested pages may already be resident; not proof execution has halted |
| Copy stays near 10 ms/frame | A repeated duration, not a progress counter |

At very low FPS, an intro can take much longer in wall time, and a short physical button press can fall entirely between guest input polls. Input latching addressed one such problem. Independent targeted tests found the tile-generation wrap and 32x64 map bugs. Neither was automatically labeled the cause of an unreplicated physical stall.

## Separate correctness comparisons from performance claims

Native tests compare optimized and ordinary execution using the same core and game sequence. CPU state, exact cycles, bus state, RAM and frame pixels are checked, not just whether both produce a picture. An independently compiled preserved PPU provided an additional renderer regression comparison. Synthetic wait-loop tests exercise IRQs and event boundaries directly.

Those tests do not establish original-GBA accuracy. V22 also passed substantial desktop checks while failing to show any hardware startup, so a native pass does not certify embedded clocks, RAM layout, IRQ interaction or boot safety.

V24 targeted a sampled busy-wait and preserved the known-working v23 startup path. The resulting board report is about 5 FPS and VBskip 15-21%, not a claimed 63% wall-time improvement from an instruction profile. Preserve that distinction when blogging about the result.

## A practical report for the next failure

Record the exact build, last visible stage, monitor status, LED pattern and whether recovery appears after watchdog timeout or reset. If the game runs, record its screen, sustained FPS, Run/Draw/Copy, VBskip, ROM misses/decode and runtime HDMI reset count. Compare at the same game scene after loading settles. Retain a confirmed-booting UF2 locally, without uploading firmware that embeds game content or the excluded core.
