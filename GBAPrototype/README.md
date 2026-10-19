# GBA Feather experiment, v28

The [main README](../README.md) covers the project and latest status. Design and chronology are in [architecture](../docs/architecture.md), [history](../docs/development-history.md), [debugging](../docs/debugging.md), [validation](../docs/validation.md) and [building](../docs/building.md).

Latest author report, 19 October 2026: Sonic Advance 3 at **7 FPS**, **PAD ON**, **VBskip 12%**, **Run 113 / Draw 22 / Copy 0 / Wait 14 ms/frame**. Scene and duration were not specified. Wait is part of Run; Draw overlaps Run. The 30/60 FPS targets remain unmet. Monitor output is 640x480/60 Hz independently of guest frame rate.

V25 reduced interpreter helper calls. V26 moved conversion and display writes onto the scanline worker, removing the full-frame PSRAM copy. V27 fixed two Thumb transfer cases and reduced fetch/timer overhead. V28 skips unchanged video-memory writes and their joins/invalidation, preserves unaffected decoded tile rows, and adds memory/IRQ measurements. See the [core audit](../docs/core-audit-v27.md) and [performance/profiling record](../docs/performance-v28.md).

The startup memory table compares SRAM 32 KiB, PSRAM 32 KiB and PSRAM 256 KiB for 15 seconds. It reports CPU-observed ns/op with HDMI enabled, not raw-chip latency. The HUD adds original HDMI-handler time in ms/s. Those board values have not been reported. PC sampling, DWT/bus profiling, a decoded-instruction cache, JIT, interpolator rendering and a replacement scanout backend remain proposals.

This directory contains v28 frontend/integration code, the original memory benchmark, ROM packer and native test harnesses. The modified `gba-embedded` CPU/PPU/bus/APU/renderer-worker core is excluded because upstream grants no redistribution licence. Generated ROM assets, firmware, source archives and game imagery are also absent. A fresh clone intentionally cannot build the complete GBA firmware or its native emulator tests; see [publication boundaries](../THIRD_PARTY_NOTICES.md).

The standalone ESP32 bridge, controller viewer and HDMI/PSRAM diagnostic sketches elsewhere in the repository do not require the excluded core and remain the independently buildable parts. The [evidence index](../docs/evidence/README.md) preserves numerical/text results through v28 without distributing core code or game data.
