# Evidence index through V45 and unfinished V46 work

Current board evidence: [V45 captures](log-analysis-v45.json),
[deduplicated overview](log-analysis-v45-overview.json),
[V45 build validation](performance-v45-summary.json),
[V44 board captures](log-analysis-v44.json), and
[V44 build validation](performance-v44-summary.json).
Saved native maps V31-V45 support exact offline symbol resolution.

[AI usage snapshot](project-metrics-2026-10-20.json) and
[per-response token CSV](project-metrics-2026-10-20.csv) use the fixed
20 October 00:30 UTC cutoff. API cost is an estimate, not a reconciled invoice.
The [memory hierarchy](memory-map-v44.json) describes V44, not a live/V45 dump.
V46 benchmark/audit evidence describes unfinished source work. It establishes
neither a ready-to-flash build, board improvement nor 60 FPS. Older notes
below retain their original checkpoint context.

# Text evidence from the local experiment

The JSON and log files here record local builds, native comparisons and later hardware reports. They contain no game payload or emulator implementation. Firmware, generated ROM data and historical core source archives are intentionally absent. Author-reported hardware values are observations, not independently instrumented benchmark measurements.

## Historical records through V28

| Build | Summary | Native or focused evidence |
| --- | --- | --- |
| v28 | [Latest hardware and build summary](performance-v28-summary.json) | [6000-frame direct-video regression](sonic-v28-direct-test.log), [CPU/fetch/video/DMA/timer audit](core-v28-audit.log), [independent PPU fixtures](ppu-v28-test.log) |
| v27 | [Core-audit summary](performance-v27-summary.json) | [6000-frame direct-video regression](sonic-v27-direct-test.log), [focused audit](core-v27-audit.log) |
| v26 | [Direct-output summary](performance-v26-summary.json) | [6000-frame direct-video regression](sonic-v26-direct-test.log), [independent PPU fixtures](ppu-v26-test.log) |
| v25 | [Interpreter helper summary](performance-v25-summary.json) | [6000-frame regression](sonic-v25-native-test.log) |
| v24 | [VBlank accelerator summary](performance-v24-summary.json) | [6000-frame regression](sonic-v24-native-test.log) |

The v28 summary includes the latest user report: **7 FPS, PAD ON, VBskip 12%, Run 113, Draw 22, Copy 0, Wait 14 ms/frame**. Scene/duration, memory benchmark, IRQ handler time, reset count and ROM miss/decode values were not supplied for this report. Unknown values remain null. Wait is included in Run; Draw overlaps Run. Changes between user reports do not establish a controlled same-scene speedup.

[STMIA before-fix failure](core-v26-before-fix.log) and [LDMIA before-fix failure](core-v26-ldm-before-fix.log) record intentional reproductions against earlier code. Their FAIL lines are expected historical evidence, not failures of the corrected v27/v28 tests. See the [core audit](../core-audit-v27.md).

## Reading older snapshots

The [development history](../development-history.md) covers earlier startup and performance results, including [v23 recovery](recovery-v23-summary.json) and [v21](sonic-v21-summary.json). Some historical summaries were written before hardware testing and retain pending/untested fields; later reports in the history and newer summaries supersede them. The original brief 7 FPS peak was corrected by the author to sustained below 5 FPS for that earlier build. It must not be confused with later v27/v28 reports around 7 FPS.

The v25 hardware version is explicitly marked inferred from conversation order. V26, v27 and v28 were named in their reports. Native tests compare paths within this experimental core; additional PPU/timer references preserve older implementations. PASS lines establish the stated regression comparisons, not original-GBA accuracy, a complete playthrough, board CPU utilization or a physical FPS speedup. Hashes identify test outputs without distributing game images. Memory/IRQ instrumentation and proposed profiling are explained in the [v28 record](../performance-v28.md).
