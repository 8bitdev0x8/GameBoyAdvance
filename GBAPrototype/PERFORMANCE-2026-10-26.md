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


## Direct-Flash instruction-window correction

The user reported an external review finding: the direct ROM provider supplied direct_data but a null code_line callback, disabling the existing cartridge/bus instruction-line leaf path. Source inspection confirms the missing opt-in. This is a confounding difference in the earlier storage comparison; the -8.54% result must not be treated as an isolated Flash-versus-PSRAM/decompression measurement.

The supplied /mnt/user-data/outputs files are not accessible in this Windows workspace. The described small fix was recreated in a separate build: an aligned full-256-byte-range-checked callback returning an immutable pointer into the raw Flash image, wired into fast_access. Misalignment, out-of-range and short-tail lines return null and retain checked fallback. No CPU, bus or guest-cycle implementation changed. No SRAM line copy is introduced; final Flash/code size must be measured rather than assumed unchanged.

The user's desktop correctness/regression and approximately25% speed results are reported evidence, not locally reproduced results. New board test: unchanged raw baseline versus callback-enabled raw candidate, same ROM/BIOS/240MHz/30MHz and policy3 frames1501..1680; exact CPU/RAM/video/cycle endpoint checks. Source: build/casper-code-line-ab-v1. The old source ZIP and measured artifacts remain unchanged. Board result pending.


### Instruction-window correction: measured board result

Fresh unchanged direct-Flash baseline: 10.49835 FPS (prior run10.49854). Callback-enabled candidate: 12.04845 FPS, +14.7651% against the fresh baseline. Both complete policy3 frames1501..1680 with identical start/end CPU/RAM/video hashes and guest cycles; final cycle471,822,400. Core Run falls90.16669 to78.32267 ms/frame. Same240MHz CPU and30MHz PSRAM, full raw ROM in Flash in both. No capture health issues. One fresh matched pair; scene is not visually verified gameplay.

Only CompressedRom.h differs among target source files, excluding generated profile maps. Static SRAM remains213,852 bytes in the measurement build. The added callback occupies36 bytes of Flash code; no SRAM instruction-line copy is introduced. Full firmware size is sector rounded, so it should not be inferred from callback code size alone.

The corrected direct build is also4.96% faster than the previous11.47897 FPS compressed run, but that is a historical comparison, not a fresh compressed-versus-fixed A/B. This demonstrates a concrete host execution-path cost; the earlier -8.54% raw storage result was confounded by the disabled fast instruction path. It does not establish that memory latency is irrelevant. No guest-cycle reductions were made. The user-reported approximately25% desktop gain is separate evidence.

Artifacts: output/GBAFeather-Casper-direct-flash-v2.uf2; output/pc-profile-map-casper-direct-flash-v2.json; output/direct-flash-code-line-v2.patch; output/casper-code-line-v2-results.json. Evidence: logs/perf-lab/casper-code-line-ab-2026-10-26_21-06-04/sequence.json. Fixed playable build installed on the serial-pinned Feather and verified through its USB runtime fingerprint and frame records. Physical monitor appearance has not been independently observed.


## CPU/system-clock experiment: invalid candidate stopped

User requested an overclock measurement before continuing the block-executor work. Isolated Casper variants keep the corrected raw-Flash instruction callback and identical policy3 frames1501..1680. The initial, subsequently rejected plan attempted nominal CPU/system clocks240 and300MHz from the existing480MHz USB PLL using the system divider; the driver-owned HSTX PLL/output remains126MHz. No voltage increase: the existing Adafruit driver requests1.15V in both.

To avoid mistaking memory-clock increases for a CPU gain, both new variants explicitly use nominal30MHz Flash and30MHz PSRAM (dividers8 versus10). The original240MHz playable build used approximately40MHz Flash; therefore this experiment's new240MHz baseline is not interchangeable with its historical12.04845FPS result. PSRAM MAX_SELECT is30 versus37 system-clock units to keep select below8us; legal RXDELAY7 and MIN_DESELECT1 retained. Source: build/casper-clock-ab-v1. The fractional system divider has finite precision; reported clocks are nominal SDK values, not an external frequency measurement. CPU/system clock changes also affect SRAM/bus timing.

The runner compares exact guest state/pixels/cycles with a clock-specific comparator, checks runtime CPU/PSRAM/HSTX rates and normal HDMI IRQ behavior, and always restores the verified240MHz Casper v2 playable firmware. One short test is not a long-term stability/thermal qualification. No60FPS prediction. Software block executor implementation remains pending after this experiment.


### Clock experiment stopped: configuration error

The new240MHz/Flash30MHz baseline completed at10.90126FPS. The300MHz candidate did not return USB or a benchmark. Source review against the clock-divider documentation found an invalid configuration:480/300=1.6, but the fully featured clock divider supports bypass1 or fractional divisors>=2. The previous assumption that any fractional divider was valid was wrong. A nominal SDK frequency would not prove a valid physical clock in this configuration.

The exact clock-test process was stopped, the invalid UF2 quarantined and runner disabled. No300MHz FPS result or stability claim exists. Automatic240MHz restoration could not run without a reachable USB device; physical BOOTSEL recovery is needed. The known-good Casper240MHz v2 firmware remains intact in output/GBAFeather-Casper-direct-flash-v2.uf2. No further clock trial is authorized by this implementation without a validated PLL/divider plan. The failure does not establish a300MHz silicon limit. Block-executor work remains pending.


BOOTSEL recovery completed: the known-good Casper v2 firmware was flashed and verified against its runtime profile fingerprint and recorded frames. USB reports CPU240MHz and HSTX126MHz. The failed300MHz candidate remains quarantined.


## Final deployed firmware after the experiments

At the user’s request, the Feather was switched from Casper back to the verified Sonic V52 USB build at240MHz. Its runtime profile fingerprint2447646605 and frame records were verified; HSTX126MHz. No new Sonic FPS benchmark was taken. This build contains its existing optimizations, not the separate native-runtime experiments. The Casper direct-Flash callback fix does not confer an equivalent gain on Sonic’s compressed provider, which already supplied instruction lines. The event-bounded block-executor redesign remains unimplemented/pending.

A complete private source-review ZIP was prepared at the user’s request, including their selected game and BIOS data. That archive remains local and is not part of this publication. The external review supplied the missing instruction-line callback diagnosis; the Windows workspace recreated the described patch and independently measured the board improvement.
