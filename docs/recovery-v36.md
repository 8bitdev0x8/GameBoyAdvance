# v36: isolate the v35 hardfault and retain its fault address

## Board failure

Subsequent result: v36 boots and runs the game. The user reports approximately
5–10 FPS, PAD ON, VBskip 18%, Run 115, Draw 38, Copy 0, Wait 37 ms/frame,
sample 205 and IFetch 96%. Scene, duration and sustained average are unknown.
The v36 UF2 is now the working fallback for v37. This does not establish the
cause of v35's failure or validate the new handler with a physical fault.

The user reports **CPU HARDFAULT on v35**, with fault status `00000000`.
The last startup stage, fault PC/LR, HFSR and whether a game frame appeared
were not supplied. V35's native tests passed, but the physical build failed;
it is withdrawn as the recommended image. The cause is not established.

For immediate recovery, the user-confirmed working fallback is
`output/GBAFeather-Sonic-v34.uf2`, SHA256
`eada8d91a8542564bc5d31e9a795acb1ee61a3a441ee75bb5c651bbb009cc603`.
V34d remains available as the diagnostic-only correction, but its board
startup has not been separately confirmed by the user.

## Isolation change

V36 restores the exact v34d `bus.cpp` and `bus.hpp`, including the generic
instruction-fetch entry point and flash placement of the ROM address-latch
reader. It therefore removes both v35's constant-width fetch specialization
and its ROM-reader SRAM relocation. Only the direct Thumb arithmetic/shift
optimization remains from v35. The existing legacy decoder remains the
reference and handles all other instructions.

This is an **isolation build**, not a claim that either removed change caused
the fault or that the remaining ALU path is board-stable. If v36 starts, the
result narrows the set of changes; if it fails, its address information is
needed before changing another execution path.

The ROM, original BIOS, BIOS/HLE policy, event/timer/DMA/APU behavior, original
16-cycle polling tail, four-job renderer, UART, clocks and linker are retained.
The corrected v34d HUD and matching sample numbers are retained.

## More useful fault recovery

The old C handler retained CFSR only. V36 adds a naked exception shim that
captures MSP/PSP and EXC_RETURN before a compiler prologue changes MSP. Its
SRAM handler uses the existing tested secure Cortex-M33 frame validator.
It records the following for the next recovery screen:

* CFSR: configurable fault status, including the previously reported zero.
* HFSR and SFSR: hardfault and secure-fault status.
* Faulting PC, stacked LR and exception-frame SP, with a frame-valid flag.
* Core number and the existing startup/runtime stage.

Unrecognized or out-of-SRAM frames are rejected before dereferencing the
stack. PC is then zero and the LR field contains EXC_RETURN; the display says
`Frame UNAVAILABLE`. The handler still uses the active exception stack;
catastrophic stacking faults can prevent a complete record. It performs no
heap allocation, serial output or display work in the fault context.

Scratch registers 2/3 retain packed fault flags/CFSR; 5/6/7 retain PC/LR/SP.
Pico SDK scratch[4] watchdog/reboot magic is preserved. In this SDK, registers
5–7 are used for a reboot-to-address only when scratch[4] contains its separate
reboot magic; normal `watchdog_enable` uses a different magic. The recovery
code reads the record before disabling the watchdog or initializing video.
The new application boot magic distinguishes v36 records from older formats.

A zero CFSR is not proof of an emulator instruction bug, a PSRAM fault,
stack exhaustion or any particular cause. The extra registers provide
evidence for the next diagnosis. The handler and recovery UI still require
physical-board validation.

## Validation

The restored bus source is checked byte-for-byte against the frozen v34d
snapshot. CPU code matches v34d after removing only the direct ALU helper
and its run-loop call; v35's constant-width fetch calls are absent. Existing
non-target sources and class layouts are checked against their snapshots.

The native audit passes 262144 ALU opcode/flag cases against the legacy
decoder, along with the existing instruction fetch, BIOS, instruction-window,
video-write, DMA and independent timer-reference tests. The exception-frame
unit test passes secure thread/IRQ, MSP/PSP, basic/FP and invalid-frame cases.

The 6000-frame ALU-only native comparison passes exact CPU/banked/flags,
cycles, bus/prefetch/latches, EWRAM/IWRAM and every output pixel/margin,
including display replacement at frame 3000. All 20 v34 checkpoints remain
identical: final PC `080bc248`, hash `5ba30470`, 1685293130 guest cycles,
11737 input reads, 953015 rendered scanlines and 42879024 direct ALU executions.
These tests do not establish physical-board stability.

Final firmware, PC-map, vectors/stack/ROM/BIOS and SRAM-copy validation are
recorded in `output/recovery-v36-summary.json`. No new FPS gain is claimed.
All modified core, ROM, original BIOS and firmware artifacts remain private.

## Trial

Flash `output/GBAFeather-Sonic-v36.uf2` with BOOT + RESET. The banner is
`BOOT v36 - SONIC / ALU ISOLATION`. If it faults, photograph the full recovery
screen, including stage, CFSR/HFSR/SFSR, PC/LR/SP and Frame status. The exact
v36 ELF and PC map are retained locally to resolve its native addresses.
The user-confirmed v34 fallback remains unchanged.
