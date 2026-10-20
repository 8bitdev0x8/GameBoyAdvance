# v30: supplied BIOS on the confirmed v28 baseline

**Subsequent board result:** the supplied BIOS boots. User readings are 5-7 FPS,
PAD ON, VBskip 12%, Run 113, Draw 30, Copy 0 and Wait 32 ms/frame; HDMI
31507 IRQ/s and 146 handler ms/s. Dependent read loops: SRAM 21 ns, PSRAM
32 KiB 767 ns and PSRAM 256 KiB 1188 ns (clarified by the user). Scene/duration
are unspecified. The BIOS swap has not resolved the reported low performance.

V29 failed on the physical board: the user reported `CPU HARDFAULT` and
`Fault status: 00000000`. The last numbered stage was not identified. A zero
captured CFSR is insufficient to identify the faulting instruction or prove its
cause. Native v29 tests passed, but that did not establish board startup.

For this experiment, all emulator core sources and existing hardware support
are restored from the preserved private v28 source snapshot. V29's source ZIP,
UF2 and logs remain available for investigation. Its extra SRAM placements,
ROM-latch optimization, display-register preflight and Exec/Event instrumentation
are absent from v30. This is a BIOS comparison against v28, not a v29 repair.

## BIOS selection

The default build still selects the existing MIT Cult-of-GBA replacement,
unchanged in `GBAFeather/src/ReplacementBios.h`, with its existing licence.
The experimental UF2 explicitly selects the user's supplied 16 KiB BIOS:

* MD5: `a860e8c0b6d573d191e4ec7db1b1e4f6`
* SHA256: `fd2547724b505f487e6dcb29ec2ecff3af35a841a77ab2e85fd87350abd36570`
* SWI vector at offset 8: `0xea00004c`

The private image is generated into ignored `src/generated/LocalBios.h`.
It is not added to the public repository, not labelled MIT, and excluded from
the new source ZIP. The ROM-containing UF2 also remains private. Only one BIOS
is linked into the experimental firmware. The previous replacement header is
preserved for a normal build and for the v28 fallback.

CPU reset starts at the BIOS reset vector (`reset(false)`). Normal BIOS SWI
dispatch remains active; this experiment does not force the partial HLE BIOS
implementation. The BIOS also executes for game BIOS calls after startup.
The game, compressed ROM, CPU/PSRAM clocks, UART, video driver, renderer,
memory layout and stack reservations remain at v28 settings.

## Rebuild

From this private workspace, run:

```powershell
./GBAPrototype/build.ps1 -Clean -BiosPath 'C:/path/to/your-private-gba_bios.bin'
```

Omit `-BiosPath` for the Cult replacement. A missing or incorrectly sized
supplied BIOS stops the build instead of silently selecting another BIOS.
Desktop game tests select the same local file through `GBA_TEST_BIOS_PATH`;
their default remains the replacement BIOS.

## Flash and fallback

Use BOOT + RESET, release RESET and then BOOT, and copy
`output/GBAFeather-Sonic-v30-OriginalBIOS.uf2` onto the RP2350 drive.
Expected banner: `BOOT v30 - SONIC / ORIGINAL BIOS`.
Stage 13 still runs the v28 memory benchmark and displays its table for
15 seconds before continuing. The original BIOS startup also differs from
the replacement's credits screen.

If startup fails, hold BOOT and press/release RESET again, then copy
`output/GBAFeather-Sonic-v28.uf2`. This is the confirmed-working fallback.
Its SHA256 remains
`faeccac5954525e7a80ae244b054e88fc28123a503ae520189d6e714f36c73bc`.
Fallback is manual reflashing: there is no automatic dual-image recovery.
Do not use v29 as the fallback after its reported hard fault.

## Validation and performance limits

The native test passed 6000 emulated frames with the supplied BIOS, scripted
input and a real renderer thread. Original-ROM/conservative and compressed-ROM/
accelerated paths matched full CPU/banked/flags/cycles, bus/prefetch/latches,
EWRAM/IWRAM and every direct RGB565 pixel and protected margin. Display target
replacement at frame 3000 also passed. The run completed 953015 scanline jobs;
final PC was `080bc248`, frame hash `5ba30470`, guest cycles 1685293130 and
input reads 11737. These checkpoints are not expected to match a different
BIOS's boot timing. The scripted test is not full gameplay validation or an
independent accuracy comparison against GBA hardware.

Firmware validation passed UF2 structure/flash bounds, vectors and stacks,
initialized SRAM against the ELF, exact packed ROM bytes and the exact supplied
BIOS once in flash, with the Cult blob absent. All existing v28 core/support
and linker files match the preserved source snapshot. The build reports
227108 static SRAM bytes and 280788 bytes of heap capacity before dynamic
allocations, matching v28. CPU and PSRAM remain at 240 MHz and 30 MHz.

The new UF2 SHA256 is
`4d3e4cd80643e2396712dd859e7956072a48ebb3d4bb25c326733e631b6add7d`.
Full validation and size information are recorded in
`output/bios-v30-summary.json`. The user subsequently confirmed Feather startup.
No controlled same-scene FPS improvement is established. Compare Run/Draw/Wait and FPS
on the same Sonic scene after startup; separate BIOS boot time from gameplay.
