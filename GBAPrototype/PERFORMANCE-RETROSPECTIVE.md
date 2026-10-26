# Performance retrospective: V1 through V49

Reviewed on 20 October 2026, Europe/Dublin. This audit uses retained development
history, user reports, exact-map board captures and primary-source reference
projects. It does not create a new firmware release. V49 remains the latest
packaged and physically tested build at the original audit checkpoint.

**Follow-up:** the first V49 board A/B/A trial subsequently measured native
mode at 5.31 FPS versus interpreter observations at 7.51 and 6.87 FPS. V50
therefore selects the interpreter by default; the connected V49 session was
switched to that mode and acknowledged it. This validates the need for the
comparison before expanding the translator. See [V50 evidence](PERFORMANCE-v50.md).

## Conclusion

The architecture has become a useful working research platform, but the record
does not establish a dependable route to generic 60 FPS GBA emulation on this
Feather. The next performance work needs a controlled board comparison and a
CPU-backend decision, rather than another bundle of unmeasured changes.

The current native translator is a valid experiment. Its benefit in real
gameplay is still unproved. A bounded code-cache redesign is worth testing
only against the existing interpreter and the same input/scene workload.

The original requirements still matter: generic ROM swapping, RP2350 Feather,
DualSense over ESP32/UART, and HSTX display. A native single-game port or a
faster host is an alternative with a changed requirement, not fulfillment of
the original target by itself.

## Recorded version history

These are observations, not comparable scores. Early versions used Yoshi;
later versions use Sonic. Early FPS values are user readings with unspecified
windows. Later values use retained binary frame intervals. Boot failures are
not assigned a fabricated zero-FPS benchmark.

| Version(s) | Recorded board result | What the evidence establishes |
| --- | --- | --- |
| V1 | Initial build, no usable gameplay measurement | Startup/video did not reach a measured working game |
| V2–V13 | Startup failures and diagnostic progress | PSRAM, allocation, stack, watchdog and video recovery work; no gameplay FPS series |
| V14 | About 2–3 FPS | First working game report, Yoshi |
| V15 | About 1–2 FPS | SRAM/cache/compiler changes did not establish a sustained improvement |
| V16 | No signal, LED off | Failed trial |
| V17 | About 3 FPS | Working recovery baseline |
| V18 | 4.6 FPS at one point; later 2.8 FPS with a stuck/malformed scene | Hot code really moved to SRAM; no stable 4.6-FPS benchmark |
| V19 | No separate reliable FPS result | Reproduced graphics defects fixed |
| V20 | About 3 FPS | Core-1 scanline renderer active; multicore alone did not solve throughput |
| V21 | Sonic below 5 FPS; initial 7-FPS report corrected as a brief peak | ROM change and working boot |
| V22 | No output or blinking LED | Failed optimization bundle |
| V23 | About 4–5 FPS | Boot restored with selected PPU changes |
| V24 | About 5 FPS | VBlank wait accelerator active, VBskip 15–21% |
| V25, inferred from sequence | About 6 FPS | User did not identify the flashed version in that reading |
| V26 | About 6–7 FPS | Direct output works; Copy becomes approximately zero |
| V27 | About 7 FPS | Run 121, Draw 33, Wait 32 ms/frame; scene unspecified |
| V28 | About 7 FPS | Run 113, Draw 22, Wait 14 ms/frame; scene unspecified |
| V29 | Hardfault | Failed CPU/bus-placement trial |
| V30 | About 5–7 FPS | Local BIOS works; no elimination of performance problem |
| V31–V33 | No separate reliable FPS figure | Native-PC sampling, BIOS reads and instruction-window work |
| V34 | User reports improvement, no numeric FPS | Some transcribed timing rows were inconsistent; HUD ownership was corrected |
| V35 | Hardfault | Failed trial |
| V36 | About 5–10 FPS | Working fallback; range across scenes |
| V37–V38 | Startup aborts | Allocation failures diagnosed before normal play |
| V39 | About 5–12 FPS | BIOS allocation fallback restores operation |
| V40 | About 4–20 FPS across frames/scenes | Renderer phase instrumentation; no fixed-scene score |
| V41 | Binary logging introduced; no single comparable gameplay score | Retained frame/profile evidence becomes possible |
| V42 | No standalone controlled board gain | Host renderer improvement is not board FPS |
| V43 | No standalone controlled board gain | Host CPU-fetch improvement is not board FPS |
| V44 | Slow retained window 6.90 FPS; longer windows 6.90–19.85 | Mixed scenes; predecoded blocks remain off |
| V45 | Slow gameplay window 7.24 FPS | Binary measurement; menu windows are faster |
| V46 | Retained windows 8.10–23.43 FPS | Scene labels mostly unknown; latest 11.17-FPS capture explicitly menu/transition |
| V47 | Confirmed gameplay 7.59 FPS, 272 frames/35.84 s | Reliable slow-window observation, no matched V46 baseline |
| V48 | Black picture with continuing execution | Native backend reaches ROM code; abnormal HDMI interrupts invalidate visible gameplay claims |
| V49 | Confirmed gameplay **10.56 FPS**, 312 frames/29.53 s | Picture restored; HDMI cadence healthy; no matched native-off or V47 scene |

V47's 7.59 and V49's 10.56 are both confirmed gameplay observations. The
arithmetic difference is about 39%, but different game scenes/input histories
prevent attributing it to a specific optimization. An overall V1-to-V49 speedup
multiplier cannot be established from this record.

Sources: [development history](PROJECT-HISTORY.md),
[V44 captures](V44-BOARD-RESULTS.md), [V45 captures](V45-BOARD-RESULTS.md),
[V46 captures](V46-BOARD-RESULTS.md), [V47 gameplay](V47-BOARD-RESULTS.md),
[V49 gameplay](V49-BOARD-RESULTS.md).

## Work that was worthwhile

- **Make the device run and failures observable.** The fixed PSRAM arena,
  stack reservation, checked allocations, persisted fault information and
  HSTX recovery produced a platform on which meaningful profiling is possible.
- **Fix actual semantic defects.** Tile-generation wrap, background-map
  addressing and Thumb multiple-transfer cases were reproduced and tested.
  They matter even when they do not improve the chosen game's FPS.
- **Remove redundant data movement.** Direct RGB565 output removed the
  approximately 10 ms/frame Copy stage in board reports. Conversion remains
  part of renderer work; not all display work disappeared.
- **Avoid proven redundant work.** Unchanged video writes, bounded idle-loop
  acceleration, instruction windows and renderer fast paths reduce particular
  costs while retaining their tested guest semantics.
- **Preserve exact evidence.** Binary logs, map fingerprints, ELF/UF2 checks,
  correctness tests and fallbacks make a failed change diagnosable.

## What the process should change

1. **Benchmark the board earlier and reproducibly.** A build that boots and
   passes desktop parity is not yet a performance improvement. Several trials
   bundled code placement, memory pressure, rendering and startup changes.
   Failures then obscured which change mattered.
2. **Treat scene identity as part of a result.** The logged game varies from
   roughly seven to over twenty FPS depending on work. Comparing a slow level
   with a menu can swamp a real improvement or invent one.
3. **Separate correctness, visibility and speed.** A version number can mark
   a fault-report improvement rather than faster emulation. A 60 Hz monitor
   signal does not establish 60 new emulated game frames per second.
4. **Require real-workload benefit from translation.** V48 synthetic loops
   execute 29–38% fewer host instructions, but do not model physical memory,
   cache pressure, compilation or HDMI interruptions. V49 spends 13.87% of
   sampled thread locations in admission/emission functions, and 4.10% in
   the native gate. Generated bodies have 0.73% of samples, with helpers
   counted separately. That exposes substantial overhead, not a measured
   eviction rate or proof that native mode wins.
5. **Use an independent accuracy reference as well as old/new parity.**
   Matching the previous core can preserve a pre-existing bug. Existing
   arithmetic oracles and selected reference-renderer tests help, but do not
   establish broad GBA compatibility or complete cycle accuracy.
6. **Keep the publication constraint visible.** The original core remains
   excluded from the public repository at the user's instruction. A public
   educational project benefits from a reproducible, distributable execution
   backend; the current support/docs-only publication is not that full release.

The practical correction is to require a matched board benchmark before
calling an experiment a speedup. Functional tests remain required, but they
answer a different question.

## The current budget

V49's confirmed gameplay averages **94.65 ms/frame**. A 60-FPS target permits
about **16.7 ms/frame**, requiring roughly **5.7 times the current throughput**
for this workload.

Run is 88.92 ms/frame, including 7.22 ms of explicit renderer waiting. The
remainder is **81.70 ms/frame**, including CPU/bus/device work and interruption.
Core-1 Draw is 16.38 ms/frame and overlaps Run. HDMI's timed handler costs
146.88 ms/s, or 14.69% of one core's elapsed time. ROM decompression averages
6.49 ms/s, about 0.65%.

Eliminating explicit render waits with other work fixed gives only **11.44 FPS**.
Eliminating the entire timed HDMI handler would ideally give about **1.17x**
speed under a fixed-other-work model. Neither calculation predicts a real
redesign. It establishes that those costs alone cannot close the gap.

## Other routes

| Route | What changes | Assessment for this project |
| --- | --- | --- |
| Improve the current M33 translator | Better admission/reuse, retained code, fewer helper/dispatch transitions, measured instruction coverage | Best bounded next experiment while preserving generic ROM loading; 60 FPS remains unproved |
| Replace the CPU/core backend | Port or adapt a more suitable interpreter/translator, preserving board integration where possible | Worth comparing with a minimal workload first; a different core is not an automatic speedup or drop-in build |
| Offline hot-block translation | Generate native code for discovered ROM regions during the PC build, with interpreter fallback | Avoids some runtime compilation; indirect targets, RAM code, precise timing and generic compatibility remain substantial work |
| DMA control-block HSTX scanout | Remove per-line CPU rearming; redesign stream/buffering | Independent engineering opportunity, with a limited direct CPU saving; memory layout might also improve but must be budgeted |
| Native port of one game | Recompile recovered/source game code and replace its GBA hardware dependencies | Can remove the guest CPU bottleneck, while giving up arbitrary-ROM emulation |
| Rendering reductions/frame skip | Draw fewer frames or reduce graphical work | Can reduce renderer load; CPU execution remains limiting, so this does not establish 60-FPS gameplay |
| More capable emulation host | Retain the controller bridge and reconsider the main processor | Appropriate if broadly compatible 60-FPS GBA playback is the firm requirement and RP2350 is flexible |

The [Pokémon Emerald RP2350 project](https://github.com/mattdeeds/pokeemerald-rp2350)
reports native game execution and DMA-ring scanout. It demonstrates a useful
alternative architecture, not generic ROM conversion. Its documented target
has 16 MB flash and an 11.7 MB game image; that image does not fit this
Feather's 8 MiB flash unchanged. Its remaining scene/audio limitations also
matter when interpreting its 60-FPS claim.

[SAT-R's Sonic Advance 3 repository](https://github.com/SAT-R/sa3)
describes a work-in-progress matching decompilation of the European game.
Its existence makes a game-specific source-port investigation plausible;
it is not a ready Cortex-M33 port or evidence that its remaining assembly,
hardware assumptions and memory footprint are solved.

The existing [mGBA review](MGBA-REFERENCE.md) identifies interpreter/fetch/timing
ideas, not an RP2350 port. Also, gpSP's inspected [ARM code generator](https://github.com/libretro/gpsp/blob/master/arm/arm_emit.h)
emits ARM-state host instructions. That backend cannot simply run on the
Thumb-only M33; an appropriate host backend and board memory integration
would be required. No external core is adopted by this audit.

## Recommended next decision

Preserve V49 and its exact gameplay logs. Establish a repeatable input replay
from reset, or at minimum a fixed scene and identical input duration, before
comparing the existing native-on/off modes. Use repeated windows, warmed
caches, frame-time percentiles and matched correctness checkpoints. Track
compiler work and code reuse with bounded binary interval counters.

Then test one cache/reuse change as a separate candidate. The current 2,304-byte
executable pool cannot keep 16 large generated blocks merely because it has
16 descriptors. A PSRAM backing tier could retain emitted bytes for promotion
to SRAM, but it competes with a nearly full ROM/data arena and needs explicit
capacity accounting. It must beat both the existing native mode and the
interpreter on the board, with no regression in correctness or stability.

If that comparison does not produce meaningful real-game benefit, revisit the
execution backend before enlarging the JIT. Keep DMA scanout as a separate
measured experiment. The long-term 60-FPS plan needs a substantial CPU-work
reduction; a sequence of small renderer or clock adjustments is insufficient
evidence that the target will be reached.
