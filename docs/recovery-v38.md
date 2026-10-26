# v38: bounded cartridge scan after the v37 startup abort

**Subsequent board result:** scanner passed, but stage 5 BIOS loading aborted.
The new caller record identifies failed `operator new` after malloc returned
null. V38 is withdrawn; see [RECOVERY-v39.md](recovery-v39.md) for the checked
SRAM-copy/flash-view recovery candidate.

## Evidence

The user reports v37 stopped at **3: SCAN CARTRIDGE**, with CFSR `00000000`,
HFSR `80000000`, SFSR `00`, core 0, PC `100254C0`, LR `1001B273`,
SP `2007FDE8`, frame valid. The retained v37 ELF maps PC to `_exit` at
`bkpt #0`. LR maps to the return from `abort` calling `_exit`.

This establishes a software abort chain, not the original reason for abort.
The stage-3 scanner allocated a temporary vector of 16 KiB plus nine overlap
bytes. Failure to obtain that contiguous heap block is the leading suspect.
The report does not prove malloc failure or exclude an assertion or earlier
memory corruption. Core 1 is not launched until stage 11, so the scanline
snapshot handoff had not started when this failure occurred.

The v37 ELF is preserved under `build/frozen-v37/` with SHA256
`460ea290ae98804dd1e73ee2bb504cf0c6d3df5715e26ac5f32ed3bb0d1b8373`.
V37 is withdrawn as a working image. The confirmed v36 UF2 stays unchanged.

## Changes

ROM feature scanning now uses a fixed stack buffer: 1024 data bytes plus the
maximum signature overlap (nine bytes). No temporary heap allocation is
needed for the scan. All eight save/RTC signatures, overlap handling,
cached results and progress callbacks remain. Callbacks run after each 1 KiB
chunk; the frontend still throttles visible progress to 256 KiB updates.
This changes startup scanning only, not game execution or rendering speed.

The firmware also wraps `abort` at link time. Future software aborts retain
the caller's return address and MSP directly, rather than only newlib's
generic breakpoint. Recovery labels these records **SOFTWARE ABORT** and
**CALLER RECORD**. They are not presented as valid exception frames. Actual
HardFault exception capture is unchanged. The abort capture uses no heap,
printing or display work, and the existing watchdog recovers it.

The v37 register-snapshot renderer is retained. CPU/bus/event behavior, ROM,
BIOS, clocks, video driver and memory arenas remain unchanged. Independent
renderer VRAM and further drawing optimizations are not included.

## Checks and limits

260 targeted scans pass with C++ new/new[] allocation denied. They compare
against whole-ROM signature searches and cover all eight markers at every
split across 1 KiB and old 16 KiB boundaries, file ends, partial tails,
ordered callbacks, save/RTC detection and cached reuse.

The full 6000-frame Sonic regression and existing core audits pass. Exact
CPU/banked/flags/cycles/bus/RAM and RGB565 pixels/margins match the reference,
including display replacement. All 20 checkpoints remain identical to v36.
The final ARM scanner stack bound and abort-wrapper linkage are checked in
`output/recovery-v38-summary.json` together with firmware/ROM/BIOS checksums.
Host tests cannot establish board stability or prove the original abort cause.

Flash `output/GBAFeather-Sonic-v38.uf2`; banner
`BOOT v38 - SONIC / BOUNDED SCAN`. If it aborts, record the new **Abort return**
address and stage. If it boots, it resumes the v37 pipeline comparison; no FPS
increase is claimed for this startup fix. The manual fallback is v36.
