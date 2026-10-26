# Performance work - 26 October 2026

## Verified previous hardware result

Native lookup cache: 3.93860 to 4.31453 FPS (+9.5448%), one matched headless pair. Full emulated PPU/APU retained; physical HDMI absent. This does not improve the currently restored V52 build or establish gameplay FPS. Both variants passed identical state/pixel/cycle endpoints. Evidence: logs/perf-lab/native-headless-pair-2026-10-25_14-51-07/sequence.json. V52 restoration verified.

## Explicit SRAM guest memory candidate

A separate cached-native candidate moves IWRAM (32 KiB) and VRAM (96 KiB) from PSRAM to named guarded SRAM arenas. Allocation uses explicit region IDs, not allocation size. Ordinary allocations retain their allocator. Guards, leases, exact layouts, drop/reconstruction and realloc refusal were tested.

Paired desktop replay passed 5,180 frame semantic and pixel/CPU/RAM checkpoints with unchanged native/service counts and zero interpreter execution. These establish correctness, not hardware speed. Private study: build/native-hot-memory-study; board integration: build/native-hot-memory-board-v1.

Board compilation and final layout checks passed: 221,000 bytes static SRAM, 16 KiB main stack, both guarded arenas within internal SRAM. QMI-direct startup helper closure remains in SRAM. The serial-pinned headless hardware replay started at 20:02 Dublin time; it restores V52 automatically. Evidence: logs/perf-lab/native-hot-memory-v1-2026-10-26_20-02-36/sequence.json. Completed: 4.61077 FPS versus cached 4.31453 FPS (+6.8661%). Both endpoint hashes and all native/service/cycle counts matched. SRAM arena guards intact, no allocation/provider failures. V52 restoration verified. This remains one headless comparison, not HDMI gameplay. No hardware gain claimed. ROM/BIOS/native artifacts remain private. 60 FPS remains unachieved.


## Casper uncompressed Flash comparison

Selected user ROM: Casper Europe, 4,194,304 bytes, SHA256 39f9e5095f9ad1ce87d24103ba383a9204ee459b1009e2d4bc6488d8c2cb0fdf. Compressed payload 1,378,988 bytes. Isolated variants use the same current emulator, prepared BIOS, clocks, scripted policy3 and frames1501..1680. Scene is not visually verified; do not call results gameplay FPS without that evidence.

The candidate embeds all original bytes contiguously in QSPI Flash and exposes immutable direct ROM access, without decompression, PSRAM page backing or SRAM hot ROM copies. Existing CPU/PPU/timing behavior is retained. Firmware verification must compare the full ROM against reconstructed UF2 Flash bytes. Paired endpoint CPU/RAM/video hashes must match before reporting a gain. Source/artifacts remain private under build/casper-flash-ab-v1. Hardware tests pending. A separate ordinary controller-enabled build will be installed only after successful A/B validation; failures restore V52.


### Casper measured result

One matched pair completed with equal CPU/RAM/video endpoint hashes and guest cycles (policy3, frames1501..1680). Compressed: 11.47897 FPS; direct Flash: 10.49854 FPS (-8.5411%). Core Run increased from 82.10015 to 90.16997 ms/frame. Draw stayed approximately 15.7 ms/frame and explicit render Wait was zero in both. HDMI interrupts remained approximately 31,500/s, with no reported capture health issues.

This trial does not support uncompressed direct Flash as a speed improvement. Flash instruction/data cache contention and loss of the existing SRAM ROM cache are hypotheses, not isolated causes. It measures ROM data placement with an interpreter; it does not measure native translated-code placement. Repeat trials are needed to quantify timing variation. The scripted scene has not been visually identified as gameplay.

Both reconstructed UF2 images contain the selected ROM exactly (compressed round-trip or complete 4 MiB raw payload). Flash used: compressed test 1,720,832 bytes; direct test 4,529,408 bytes. All ROM bytes fit in physical Flash. Playable deliverable: output/GBAFeather-Casper-direct-flash-v1.uf2, profile map output/pc-profile-map-casper-direct-flash-v1.json. Playable direct-flash firmware installed on the serial-pinned Feather. USB runtime dump matched its profile fingerprint and contained frame records; HDMI hardware status was responsive. Physical picture/gameplay still requires visual confirmation. Evidence: logs/perf-lab/casper-flash-ab-2026-10-26_20-27-44/sequence.json.
