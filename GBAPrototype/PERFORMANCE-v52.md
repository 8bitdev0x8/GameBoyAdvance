# V52: make hardware experiments repeatable

20 October 2026. V52 changes the original hardware/support code for automated
experiments. **Its CPU, bus, PPU and other emulator-core files are identical to
V51.** It is not a new performance optimization or a claim of 60-FPS gameplay.

## New V51 observations

| Capture time, Dublin | Retained FPS | Run | Draw | Wait |
| --- | ---: | ---: | ---: | ---: |
| 20:24:17 | 14.40 | 65.17 ms | 7.91 ms | 3.88 ms |
| 20:26:10 | 13.23 | 70.94 ms | 16.90 ms | 8.06 ms |
| 20:26:24 | 8.86 | 106.52 ms | 34.85 ms | 16.49 ms |

These are different windows, and the last two overlap in guest-frame numbers.
They are not independent repeated trials. The last has 279 frames (4,491–4,769)
over 31.48 seconds, giving Run-minus-Wait **90.03 ms/frame**. It remains much
larger than 16.7 ms. The earlier confirmed V50 demo was 7.46 FPS and 109.83 ms
outside Wait, but the recordings do not establish a controlled V50/V51 speedup.

## Automation bugs found by using the hardware

The initial smoke trial preserved the current ring, then exposed two problems:

1. Windows can return “device does not exist” during the 1200-baud COM-port
   change because reset has already succeeded. The uploader previously aborted.
   Backend 1.0.1 now probes the pinned USB serial in BOOTSEL before deciding
   whether the reset failed. It still refuses to flash a different device.
2. V51 retained an interrupted-run marker through intentional resets. The next
   application boot could stop on its recovery screen with no USB command
   processing. V52 acknowledges `prepare flash` with `FLASH READY` at a drained
   frame boundary, clearing that marker before reset. If the host abandons the
   upload, the marker is rearmed after five seconds; the watchdog remains active.

The initial failed trial and recovery timeout remain in
`logs/perf-lab/smoke-2026-10-20_20-32-14-788008/trial.json`. A serial-specific
1200-baud touch and application reboot restored V51 without a physical button
press. The V52 upgrade's first boot can inherit V51's old marker, so that
one-time bootstrap is recorded separately in `logs/perf-lab/bootstrap-v52/`.
It must not be presented as a clean first-attempt automated acceptance.

## Validation and layout

- 15 mocked performance-lab tests, 13 uploader tests (including the two new
  reset regressions), and 9 existing benchmark tests pass.
- The real automated host-check stage rebuilds and passes the core audit and
  6,000-frame compressed-ROM/threaded RGB565 replay with the selected BIOS.
  Its final frame hash is `5ba30470`, PC `080bc248`.
- The final two-pass firmware map has 1,061 exact ranges, fingerprint
  **2,447,646,605**. The UF2 contains the expected packed game and BIOS, and
  initialized SRAM matches the ELF load image.
- Program: 5,029,672 B. Linked static SRAM: 245,488 B. Space between linked
  heap start and reserved main stack: 262,408 B, before runtime allocations.
- V51 remains intact as the fallback. Hashes and layout evidence are in
  `output/performance-v52-summary.json`; hardware results are recorded separately.

See [performance automation](PERFORMANCE-AUTOMATION.md) for commands,
timestamped evidence, recovery limits and the distinction between timed boot
observations and deterministic gameplay replay. The workflow deliberately does
not automatically promote a candidate based only on an observed FPS increase.

## Normal V52-to-V52 acceptance

The final normal smoke test, `smoke-2026-10-20_20-43-45-012414`, completed at
20:47:00 Dublin time. It received `FLASH READY`, reset through COM9, loaded and
verified the pinned Feather, booted normally and downloaded healthy telemetry.
No physical button or recovery detour was required. The known V52 baseline
remains running. The 5-second warmup/10-second capture averaged 16.54 FPS in
an early boot/demo window; **this is not a gameplay speedup measurement**.

The standard configuration uses 30 seconds of warmup and 20 seconds of recording
after firmware readiness. It does not select a fixed guest-frame range. A
deterministic input/frame replay is still required for automatic gain acceptance.
