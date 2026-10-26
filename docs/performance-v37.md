# v37: immutable scanline register snapshots

**Board result: FAILED during stage 3 cartridge scan.** PC `100254C0` and
LR `1001B273` resolve to `abort -> _exit -> bkpt #0`; the original abort caller
is unknown. The renderer had not started. V37 is withdrawn; see
[RECOVERY-v38.md](recovery-v38.md) for the bounded-scan recovery candidate.

V36 boots on the board. The user reports approximately 5–10 FPS, PAD ON,
VBskip 18%, sample 205: Run 115, Draw 38, Copy 0, Wait 37 ms/frame and
IFetch 96%. Scene, duration and sustained average are unspecified. The two
reported CPU-dispatch percentages (16% and 15%) are retained as separate
reports; the v36 map has only one matching record, so they are not summed.

Run includes waiting and HDMI preemption. Draw overlaps Run on core 1.
Subtracting 37 from 115 leaves about 78 ms of core-0 elapsed work, not a
measurement of pure interpreter time. Eliminating renderer waiting alone
would therefore not reach the 16.7 ms budget for 60 FPS.

## What changed

Previously each queued job pointed to live PPU registers. Even a changed
scroll/window/blend register forced core 0 to drain all queued rendering.
Each v37 job captures an immutable render-register record, including display
mode, BG configuration/scroll/affine references, windows, mosaic and blend
state. The consumer's PPU helpers read that record rather than live registers.

Register changes increment a producer-owned generation. The consumer applies
dirty-line invalidation when it encounters the new generation in FIFO order.
Earlier jobs keep both their older registers and the earlier dirty-state
history. The producer never modifies the consumer's dirty flags or caches
while queued jobs run. Acquire/release queue publication protects each record
until the consumer releases its slot.

An explicit backend capability enables this behavior. Backends without the
capability keep the existing conservative register barriers. Reset and
backend/display changes still drain. Actual changes to shared VRAM, palette
and OAM, direct video-memory spans and DMA retain their original barriers.
There is no second copy of video RAM in this build.

The four-entry queue remains. Draw timing now includes snapshot application,
dirty bookkeeping, rendering and RGB conversion, including bookkeeping for
clean jobs. Wait remains split into Qfull and Barrier with the same sample ID.
CPU/interpreter, BIOS policy, game ROM, clocks, UART, scanout, event timing,
stack reservations and fault recovery are retained from v36.

## Why this is not a promised large FPS gain

The controlled desktop Sonic workload shows that snapshots remove almost all
register-drain waiting, but much of the remaining blocked time is in a full
queue. The initial three-run median total host time was approximately
1689 ms for v36 versus 1697 ms for snapshots: no meaningful speedup. Median
explicit barrier time fell from about 144 ms to 1.25 ms over 2400 measured
frames, while full-queue waiting grew. Desktop thread scheduling and memory
behavior differ from RP2350; these values are not board FPS predictions.

A sixteen-entry snapshot queue and an opaque-compositor shortcut were also
tested. Neither established a consistent total-time improvement, and both
are excluded. There is no frame skipping or ROM patch in v37.

Independent renderer video memory could remove changed-memory barriers, but
requires about 98 KiB plus ordered updates, and frame-boundary scheduling
would also need attention. It cannot itself make drawing faster or eliminate
queue-capacity waits. Putting the renderer's frequently read data in PSRAM
could be a regression. It is an option, not a proven dominant bottleneck.

## Validation

* 38,400 scanlines with four different states queued before consumption and
  concurrent live-register writes match the independent v21 PPU. Tests cover
  all graphics modes, forced blank, affine references, scrolling, windows,
  effects, byte/word merges and protected RGB565 margins.
* The independent 240-fixture PPU comparison passes all graphics modes,
  sprite/affine cases, text sizes, windows, effects, cropped/full output,
  changed video memory and repeated frames.
* A backend-capability audit checks 2000 register writes with guest-bus parity,
  no register joins, retained changed VRAM/palette/OAM joins, and restoration
  of conservative behavior when switching backends.
* The core's CPU/fetch/BIOS/instruction-window/video/DMA/timer audits pass.
  The full 6000-frame threaded Sonic comparison preserves exact CPU/banked
  state, cycles, bus/latches, EWRAM/IWRAM and every output pixel, including
  target replacement at frame 3000. All 20 frozen v36 checkpoints match.

Final firmware, class/SRAM/stack checks, source preservation, exact ROM/BIOS,
final PC map, checksums and logs are recorded in
`output/performance-v37-summary.json`. These tests do not establish physical
board performance or stability. Modified core, ROM, BIOS and UF2 remain private.

## Board comparison

Flash `output/GBAFeather-Sonic-v37.uf2` using BOOT + RESET. The banner is
`BOOT v37 - SONIC / REGISTER SNAPSHOTS`. Use the same Sonic screen for at
least 20 seconds. Record FPS and the matching sample IDs for:

* Run / Draw / Copy / Wait.
* Qfull / Barrier.
* HDMI IRQ ms/s and reset count if available.

High Qfull points to consumer throughput or burst scheduling. High Barrier
points to remaining shared-memory/frame-boundary drains. A low Barrier alone
does not prove a total FPS improvement. The unchanged, user-confirmed v36
image is the manual-reflash fallback; its new fault handler has not yet been
validated by a captured physical fault.
