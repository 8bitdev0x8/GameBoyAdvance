# V44: renderer specialization and a bounded CPU block experiment

V44 is an experimental local build for the same Sonic cartridge, supplied
BIOS, 240 MHz CPU and 30 MHz PSRAM settings. It preserves V43's fixed-width
fetch work. It does not establish a large FPS improvement or real-time
emulation. Subsequent board captures confirm operation at 6.90–19.85 FPS
across retained windows; an identical-scene improvement is not established.
See [V44-BOARD-RESULTS.md](v44-board-results.md) for seven captures, stability
observations and the remaining CPU/background/compositor bottlenecks.

The image is `output/GBAFeather-Sonic-v44-USB.uf2`, banner
`BOOT v44 - SONIC / BLOCK TRIAL`. Keep
`output/GBAFeather-Sonic-v43-USB.uf2` as the unchanged fallback; V42 is also
preserved. Modified core, cartridge, BIOS and firmware remain private.

## Renderer changes

The compositor chooses an ordinary no-effect path once per constant-window
span. Its opaque search is specialized for zero through four background
layers, with separate object-present/object-absent cases. The compiler can
unroll the short search and omit effect tests or object/priority loads that
cannot contribute. Forced translucent OBJ blending still uses the existing
second-layer path even when window effects are disabled. Background ordering,
window boundaries, transparency sentinels, priority ties and pixel values
must match the reference implementation.

The ARM compiler inlines the opaque specializations into the SRAM-resident
`Ppu::render_prepared_scanline`; standalone opaque function symbols are not
required. This changes computation within a job, not queue capacity or shared
memory ownership. Changed VRAM/palette/OAM writes still drain pending reads.

Controlled native MSVC /O2 results, three rotating-order repeats, same pinned
logical CPU and high process priority, blocks disabled in both variants:

| Replay warmup / measured frames | Total time change | Draw time change |
| --- | ---: | ---: |
| 0 / 1,800 | 1.01% less | 1.63% less |
| 3,600 / 1,200 | 2.50% less | 2.48% less |
| 12,000 / 600 | 1.30% more | 1.27% more |

These are small, scene-dependent host results, including a regression.
They are not an RP2350 FPS estimate or a consistent demonstrated speedup.
The prior effects-only experiment was approximately neutral and is retained
as `build/gba-host/render-v44-effects-only.json`. The selected experiment's
raw runs and medians are in `output/render-v44-host-benchmark.json`.

See [RENDER-OPTIMIZATION-MAP.md](render-optimization-map.md) for the larger
pipeline diagram and measured priorities: background drawing, compositing,
queue bursts, dirty tracking, memory versioning and HDMI IRQ handling.

## Predecoded Thumb blocks

This is bounded predecoding and grouped interpretation, **not native dynamic
translation**. It supports register-only Thumb ALU encodings `0000–3fff`:
immediate shifts, register/immediate ADD/SUB, and immediate MOV/CMP/ADD/SUB.
ARM, branches, memory instructions, MMIO and non-ROM execution continue
through the ordinary interpreter.

Thirty-two direct-mapped 32-byte ROM lines store decoded operand fields
for up to sixteen instructions each. Entries are learned from executed
opcodes; there is no speculative ROM read or decompression. The cache takes
3,328 bytes, plus counters/control in the emulator object. Its scope is
canonical cartridge ROM above the header, with each actual opcode checked
against its decoded entry. Changed opcodes invalidate cached entries and
execute through the appropriate ordinary path.

Every instruction still performs its architectural fetch and guest bus-cycle
accounting. Blocks stop at the slice deadline, cache-line boundary or normal
hardware-service countdown. DMA becoming due is serviced at the same point
after fetching; block execution then exits to full IRQ/halt/prefetch checks.
This preserves timing rather than skipping required device behavior.

An earlier eager-lookahead implementation passed tests but regressed CPU-only
host time by about 17–19%. Learning only executed entries reduced that cost,
but the final experiment still regresses:

| Workload | More host time with blocks enabled |
| --- | ---: |
| CPU only, 3,600 warmup + 2,400 frames | 10.72% |
| CPU only, 12,000 warmup + 1,200 frames | 8.30% |
| CPU plus synchronous renderer, 3,600 + 2,400 frames | 4.84% |

Across the 6,000-frame probe, about 29.4 million block entries execute about
40.1 million decoded instructions, averaging only 1.37 instructions per
entry. Short sequences, cache lookup/validation, entry calls and counters
offset the saved decode/outer-loop work. Preserving the complete per-opcode
fetch also leaves much of the measured bus cost intact. A larger translated
block design may differ substantially; this narrow experiment cannot prove
that all block execution or native recompilation would be slow.

**CPU blocks default OFF.** They are available for a board A/B experiment,
not silently enabled as a claimed performance fix. Status is visible as
`B0`/`B1` on the top HDMI line. The function map labels the optional executor
`Thumb block execute`.

## Switching and capturing on the board

Close other programs using the Feather's USB COM port. The ESP32's controller
UART/COM port is separate. V44 accepts commands at a completed-frame boundary:

```powershell
python <workspace>/GBAPrototype/tools/block_control.py --port COM9 --mode off
# Play a repeatable slow scene for at least 30 seconds, then capture:
python <workspace>/GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v44-blocks-off.bin --open

python <workspace>/GBAPrototype/tools/block_control.py --port COM9 --mode on
# Repeat the same scene and capture a separate window:
python <workspace>/GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v44-blocks-on.bin --open

python <workspace>/GBAPrototype/tools/block_control.py --port COM9 --mode status
python <workspace>/GBAPrototype/tools/block_control.py --port COM9 --mode off
```

The control helper confirms mode and reports cumulative u32 lookups, hits,
misses, entries/batches, decoded instructions and opcode mismatches. An
optional `--output new-file.json` preserves that status without overwriting
an existing file. Compare counter differences across a measurement window;
enabling/disabling does not reset the warmed cache or cumulative counters.

The existing binary format remains schema 1. Frame flag bit 32 records CPU
block mode and the decoder exports `blocks_on`; block counters themselves
are queried separately, not recorded per frame. Mode transitions reset the
frame measurement baseline and mark the crossing CPU sampling window as
contaminated, using the existing gap flag. Raw commands are `blocks on`,
`blocks off`, `blocks status`. Status replies are bounded to one USB packet
and only sent when space is available. The helper retries queries; commands
are ignored during binary export to avoid mixing ASCII and capture bytes.

## Correctness, size and limits

- 262,144 individual ALU fixtures compare all supported encodings, NZCV
  inputs, edge operands and register aliases against the ordinary decoder.
- 20,480 block slice fixtures preserve exact CPU/banked state, flags,
  cycles, bus/prefetch/latches, IRQ/deadlines, DMA channels and RAM, including
  timer interrupts, prefetch on/off, mid-block DMA and a changed opcode.
- The full core audit also retains fixed-width reads, BIOS bounds,
  instruction-window replacement, video writes, DMA and timer regressions.
- 480 PPU fixtures and 38,400 queued register snapshots match the independent
  renderer across graphics modes, windows/effects, sprites, video writes,
  cropped/full output, direct RGB565 and protected margins.
- 6,000 Sonic frames with blocks enabled on the compressed/threaded path
  match the conservative original-ROM path and all twenty V43 checkpoints.
  Final PC is `080bc248`, video hash `5ba30470`, cycles `1685293130`, with
  953,015 worker scanlines. Frame/profile log accounting is checked.
- Thirteen Python protocol tests include the new mode flag and status parser.

Final ARM static SRAM is 244,252 bytes (4,528 more than V43). The emulator
object is 86,912 bytes (3,352 more). The linker leaves 263,644 bytes of heap
capacity before dynamic allocations; that is not runtime free memory.
CPU dispatcher and block executor have individual stack frames of 80 and
104 bytes. The final map has 1,037 function ranges. Full UF2 validation,
checksum, exact map fingerprint and class sizes are in
`output/performance-v44-summary.json`.

The ROM cache and PSRAM arena sizes are unchanged. Additional SRAM use can
reduce room for an optional BIOS copy; the checked flash-view fallback and
8 KiB heap reserve remain. See [MEMORY-LAYOUT.md](memory-layout.md).

The board has now booted successfully and supplied valid logs, after an
earlier black-screen report. Its cause is not established by the captures.
Controlled real-board speedup, instrumentation overhead and memory stalls
remain to be measured. Native translation, batched architectural fetch
accounting, dependency-based redraw, video-memory versioning and relocating
HDMI IRQ handling have not been implemented in this build.
