# 25 October 2026 optimization work

Times are Europe/Dublin. This is an execution record; prepared/compiled work
is separated from physical results. All game-derived assets remain private.

## Completed: Native HDMI Test 3

The incomplete Test 3 was audited and frozen. The ELF contains all 11 retained
DVI timing records in SRAM, identical to Test 2; QMI startup helper closure and
native runtime/ROM/BIOS/ABI checks pass. The private UF2 SHA256 is
`3d5438e80ade31d7162954427cbda45c85abb0b3068efd6ef79ee78548ce6ac6`.

The physical test ran at 240 MHz CPU/30 MHz PSRAM. Image verification passed;
scanout was about 31,472 IRQ/s during loading, then 84,066 IRQ/s during ROM
metadata scanning. The framebuffer still held non-black pattern/HUD pixels.
The rate failure preceded guest instruction execution. Runtime construction
passed and guest frames 1-10 progressed while final diagnostics were captured.
The runner stopped before a measured FPS window and successfully restored V52,
verifying runtime map fingerprint 2447646605 and the fallback dump hash.

Evidence: `logs/perf-lab/native-runtime-hdmi-v3-2026-10-25_14-12-36/`.
No FPS improvement or visible-monitor success is claimed for Test 3.

## In progress: clean native baseline/cache A/B

Two new headless variants keep all emulated CPU/PPU/APU/event work, ROM/BIOS,
native image and strict 5,000+180-frame policy-3 workload identical. Both lookup
callbacks and aggregate counters are in SRAM. Neither ELF contains the physical
HSTX driver or a core1 worker. The cached variant adds 17,408 bytes of cache
data and 128 bytes of SRAM code. The exact consumed header passes the existing
10,270,251-key trace tests; this is a lookup correctness/search-work result,
not a board FPS result.

Root owns a single serial-pinned batch: baseline, then cached only if baseline
passes, then one final V52 restoration. HDMI is intentionally absent during the
pair. No status commands are sent during either measured window. All native
entry/cycle/service/lookup counts and endpoint hashes must match.

The batch completed at 15:26 on 25 October. Baseline: 3.93860 FPS; cached: 4.31453 FPS (+9.5448%). Both completed identical 5,180-frame work with matching endpoint hashes, native/service counts and guest cycles; no interpreter fallback. Cache resolution calls fell from 669,616 to 1,713 in the measured window. This is one headless pair, not HDMI gameplay or a variance study. V52 restoration was verified by runtime fingerprint 2447646605.
Evidence: `logs/perf-lab/native-headless-pair-2026-10-25_14-51-07/`.

## Work in separate private trees

- **Video recovery/instrumentation (V4):** producer/drain/quiescence handshake,
  owner-only driver reset/rebind, DMA chain disable-before-abort, bounded retry
  policy, pending-channel/IRQ-gap diagnostics and invalidation of recovered
  benchmark windows. No buffer may be freed before core1 acknowledges it has
  stopped accessing it. This is implementation work, not a passed board fix.
- **Hot guest memory:** explicit named IWRAM/VRAM SRAM arenas with allocation
  provenance and unchanged runtime layout. The study must preserve host replay
  hashes/cycles and account for the full SRAM cost. No board gain is claimed.

## References and promotion gates

The [reference-guided plan](REFERENCE-GUIDED-OPTIMIZATION.md) records sources
verified on 25 October, including the downloaded official RP2350 datasheet and
its SHA256. Public instruction/memory test suites are identified; no new suite
pass is claimed. Existing independent mGBA compatibility differences remain.

Promote only matched board gains with correct state/pixels/cycles, bounded
memory/stack use and stable visible output. Headless throughput is an upper
bound and must not be labelled HDMI gameplay FPS. 60 FPS remains unachieved.
