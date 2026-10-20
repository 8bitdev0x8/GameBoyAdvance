# Local emulator integration and original support tools

This publication contains original hardware integration, diagnostics, decoder,
profiling and test harness code. The modified upstream core, private cartridge,
local BIOS, generated assets and compiled firmware are intentionally absent.
This is not a complete downloadable or rebuildable emulator release.

The latest packaged private candidate is V46; V45 is the latest board-measured
build. Slow V45 gameplay remains around seven FPS. V46's cached-fetch leaf and
full mapped native-PC histogram export pass native/firmware checks, but board
boot, performance improvement and the 60 FPS target are not established.
See the [V46 report](../docs/performance-v46.md).

Start with the [project README](../README.md),
[full history](../docs/development-history.md),
[architecture](../docs/architecture.md),
[V45 board evidence](../docs/v45-board-results.md),
[binary log guide](../docs/debug-log-v41.md),
[AI metrics](../docs/ai-project-metrics.md), and
[publication boundaries](../THIRD_PARTY_NOTICES.md).

Standalone controller/video sketches can be built separately. Emulator build
and CMake entry points stop with an explicit explanation when the excluded
core is missing. Original Python log tools and accounting tests can run without it.
