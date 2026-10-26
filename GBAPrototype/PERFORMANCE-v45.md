# V45: packed color math and direct text-row output

This local Sonic candidate retains 240 MHz CPU, 30 MHz PSRAM, the supplied
BIOS, four-job renderer queue, HSTX output and USB binary logging. Subsequent
board captures confirm operation but slow gameplay remains around seven FPS.
See [V45-BOARD-RESULTS.md](V45-BOARD-RESULTS.md); no controlled same-scene
V44/V45 improvement is established.

## Selected renderer work

- Alpha blending packs red and blue into separate arithmetic lanes and handles
  green separately. Channel rounding and saturation remain exact.
- Brighten/darken operations use packed lanes while preserving all 17 effect
  coefficients and bit-15 behavior.
- Active text-background rows overwrite transparent pixels with the original
  sentinel while drawing. Their preceding blanket clear is removed. Non-text
  background generators retain the clear they require.

No new framebuffer, cache, queue capacity or render-memory allocation is added.
The ARM compiler required an explicit `std::min<u32>` type because its `u32`
differs from the host compiler's underlying unsigned type; the corrected code
is included in the firmware build and has the same arithmetic semantics.

## CPU experiments and defaults

The common empty-ROM-prefetch transition is computed directly, retaining the
generic path for unusual states. An optional bounded uncached Thumb ALU burst
continues through consecutive eligible instructions without repeating the outer
dispatch checks. Each instruction still performs the architectural fetch and
timing; DMA, interrupts and slice boundaries retain their service points.

The uncached burst defaults **off** because controlled host runs establish no
dependable CPU gain. V44's cached predecoded blocks also remain **off**.
Neither is native JIT translation. Runtime A/B commands are:

```powershell
python GBAPrototype/tools/block_control.py --port COM9 --engine burst --mode status
python GBAPrototype/tools/block_control.py --port COM9 --engine burst --mode on
python GBAPrototype/tools/block_control.py --port COM9 --engine burst --mode off
```

Commands apply at a completed-frame boundary. `burst status` returns `BST 0`
or `BST 1`; enable this engine only with blocks off. Frame flag 64 exports
`alu_burst_on`; the existing block flag remains 32. Mode changes reset the
frame-time baseline and mark the crossing PC window as contaminated. Export
and ASCII status are kept separate.

## Controlled native results

Four compile variants isolate the older CPU/render paths, new CPU paths,
new renderer paths, and both. All use the current source, MSVC /O2, three
rotating-order repeats, one pinned logical CPU and high process priority.
Timing variants omit host ALU instrumentation counters. The first instrumented
run is retained separately and is not mixed into these medians.

Positive percentages below mean less elapsed host time:

| Workload warmup / measured frames | Variant | Total time reduction | Draw reduction |
| --- | --- | ---: | ---: |
| Compute 3,600 / 2,400 | CPU experiment | −0.29% | N/A |
| Inline rendering 3,600 / 2,400 | Renderer only | 3.73% | 10.78% |
| Inline rendering 3,600 / 2,400 | CPU + renderer | 4.05% | 12.12% |
| Inline rendering 12,000 / 600 | Renderer only | 1.12% | 4.21% |
| Inline rendering 12,000 / 600 | CPU + renderer | 0.11% | 4.31% |

The later CPU-only variant takes 0.74% more total host time than its baseline.
Compute-only results vary slightly even when the renderer is unused; code
layout and run-to-run variation remain. These are modest candidate changes,
not evidence of a large RP2350 gain. The shipped default also retains the
closed-form prefetch change, so the renderer-only ablation is not an exact
timing forecast of the entire default firmware.

All variants match guest cycles, PC, CPU checksum and video checksum in each
workload. Raw runs/medians: `output/v45-host-benchmark.json`.

## Correctness and release checks

- 2,228,224 exhaustive brightness outputs and 1,000,000 alpha fixtures match
  the scalar reference, including saturation, rounding and bit 15.
- 20,480 slice fixtures plus 2,000 mixed-instruction slices independently
  cover each optional CPU engine, including prefetch, IRQs, DMA and changed
  ROM opcode fallback. The full core audit has 13 passing groups.
- 480 independent PPU fixtures and 38,400 queued register-snapshot scanlines
  compare modes, windows/effects, sprites, live register writes and output.
- The default-off CPU-burst candidate matches all 20 V44 checkpoints in
  6,000 threaded/direct-output Sonic frames: final PC `080bc248`, video
  hash `5ba30470`, guest cycles `1685293130`, 953,015 worker scanlines.
- Protocol tests cover the new mode flag/status parser; accounting tests
  separately validate the blog's AI token/time metrics.

Retain the exact V44 UF2 as the fallback. Its SHA256 is
`3bc324324479a28c5d8e353709bfc5e85fbfca8c0357db1f4f07192a35dbed1d`.
The candidate is `output/GBAFeather-Sonic-v45-USB.uf2`, SHA256
`772fb3c3dcdfd3927f85aa4c2819ebe2ce7a63ca46dcc63af6c8970ec6d1f8b9`.
Final ARM static SRAM is 245,700 bytes (+1,448 versus V44), emulator size
86,920 bytes, CPU dispatcher stack frame 88 bytes. Its 1,037 native function
ranges match the final ELF, map fingerprint `1969481180`. The heap capacity
before dynamic allocation is 262,204 bytes; runtime free heap is not measured
for V45. Final vectors, stack/flash bounds, initialized SRAM, exact ROM and
single BIOS copy pass UF2 validation. The full record is
`output/performance-v45-summary.json`. The private ROM, BIOS, modified core and firmware are not
part of the public support-code repository.

Compare V44 and V45 on the same slow screen with the same input sequence,
at least 30 seconds of retained frames per build, both optional CPU engines
off, exact symbol maps and matching scene/warmup. Current logs do not establish
60 FPS or full-game compatibility.
