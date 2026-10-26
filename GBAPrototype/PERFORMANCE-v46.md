# V46: CPU fetch trial and complete mapped native-PC histograms

The V45 gameplay captures remain around seven FPS. The latest slow window is
138.17 ms/frame, with Run 131.48 ms including 14.70 ms of renderer waiting.
The target is **16.7 ms/frame / 60 FPS gameplay**. This build does not establish
that target. See [V45-BOARD-RESULTS.md](V45-BOARD-RESULTS.md).

## Execution-path change

V46 tries a small inline fetch leaf for an opcode already in the stable SRAM
instruction line. It avoids calling the general specialized fetch when all
its preconditions are met: sequential CPU fetch, ordinary first ROM window,
aligned access, a valid cached line and uncomplicated empty/buffered prefetch
state. It computes the same fetch cycles, prefetch transition, open-bus value,
last-access bookkeeping and instruction-hit counter.

Cache misses, partial ROM tails, GPIO/header addresses, EEPROM, alternate ROM
windows, hint boundaries, RAM/BIOS accesses, DMA and other states retain the
existing fetch path. No opcode, guest cycle, frame or timer service is skipped.
This is not ROM recompilation or native JIT translation.

The leaf defaults on for the board trial; cached predecoded blocks and the
uncached ALU burst remain off. USB A/B control applies at a frame boundary:

```powershell
python GBAPrototype/tools/block_control.py --port COM9 --engine fetch --mode status
python GBAPrototype/tools/block_control.py --port COM9 --engine fetch --mode off
python GBAPrototype/tools/block_control.py --port COM9 --engine fetch --mode on
```

`FET 0` / `FET 1` confirms the mode. Frame flag 128 records `leaf_fetch_on`.
Transitions reset the measurement baseline and flag the crossing CPU window.
V46 also fixes an extra conversion specifier in the older `blocks status`
formatter, which could prevent a valid bounded status reply.

## Host results are modest and mixed

Three alternating-order repeats use one pinned logical CPU, high priority,
MSVC /O2, the same source and the same ROM/BIOS. Blocks/burst are off in both
variants; the baseline compiles out the leaf. Host instrumentation counters
for the new leaf are omitted in timing binaries.

| Warmup / measured frames | Workload | Total host elapsed change |
| --- | --- | ---: |
| 3,600 / 2,400 | CPU only | 3.07% less |
| 3,600 / 2,400 | CPU + inline renderer | 1.60% less |
| 12,000 / 600 | Later inline-render workload | 3.43% more |

Guest cycles, PC and CPU/video checksums match within each workload. Renderer
timing also varies even though the PPU source is unchanged; code layout and
run-to-run effects limit attribution. These are not RP2350 measurements or a
prediction of a large gain. The replay reaches entry/menu states and does not
represent the reported slow gameplay scene. Raw results are in
`output/v46-host-benchmark.json`.

## Find the rest of the CPU time

Previous captures retained only three native function names and rounded shares
per five-second window. V46 also writes **every nonzero mapped function's exact
sample count**. It reuses the existing bounded SRAM histogram and 24 KiB PSRAM
log ring. A small chunk writer replaces a second full histogram copy; no new
large buffer or heap allocation is added.

The existing sampler ISR is unchanged. It still captures the native M33 PC at
roughly 485 Hz with a dithered interval. Histogram serialization happens in
ordinary core-0 code after sampling, with only the sampler IRQ masked; HDMI
continues. Its elapsed overhead and shorter retained log duration must be
measured on the board. Sampling retains interrupt-delivery bias: these are
native sampled locations, not guest instruction counts or perfect stall/GPU
attribution. Unmapped/invalid samples and separately timed HDMI IRQ cost remain.

Schema-1 **type 3** records contain 16 sparse u32 fields: frame, window length,
sample count, flags and six function-ID/count pairs. Flags 4/8 mark the first
and final chunk; bit 2 marks an export/mode-contaminated window. A final marker
is emitted even for an empty or exactly full last chunk. The decoder marks a
window incomplete if its leading chunks were overwritten; partial windows must
not be treated as complete distributions.

The updated PC decoder retains frame/profile CSVs and also writes
`*-cpu-histogram.csv`, with full function name, hit count, percentage,
export-contamination flag and window-completeness flag. JSON retains these rows.
The JSON summary also aggregates functions over complete, uncontaminated windows, using each window sample count only once; unmapped samples remain in the denominator. Older V41-V45 captures remain supported. Older decoder versions reject the new
record type, so use the updated tool for V46 captures.

## Capture the next evidence

After flashing V46, keep a repeatable slow gameplay scene active for at least
20 seconds and capture using the Feather USB COM port:

```powershell
python GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v46-fetch-on --open
```

Then switch fetch off, return to the same scene and warm it for the same time,
and capture `--output sonic-v46-fetch-off`. Repeat on/off to check variability.
Do not compare a fast entry/menu window with a slow gameplay window. Keep V45
as the unchanged manual fallback.

The complete mapped distribution can establish how much of the remainder is
bus reads/writes, interpreter opcode work, IRQ/timer/DMA/APU service and other
functions. Further guest-PC/address profiling, controlled device-cost isolation
and larger execution backends remain separate work. DWT/XIP/bus attribution,
native translation, VRAM versioning and an alternative scanout backend are not
implemented by this trial.

## Validation checkpoint

The focused audit includes 160,000 leaf-fetch comparisons, 85,434 eligible
cached hits, prefetch on/off, aliases/hint edges, tail bounds, failed providers,
GPIO, BIOS/RAM, DMA, EEPROM and provider replacement. Existing ALU, burst/block,
bus, video and timer audits remain, for 14 passing groups. Binary writer tests
cover empty, partial and exact-multiple chunks; the C++ fixture and 14 Python
protocol tests verify exact counts, padding and incomplete-window detection.

The full 6,000-frame CPU/cycle/bus/RAM/pixel regression passes all twenty V45
checkpoints: final PC `080bc248`, framebuffer hash `5ba30470`, guest cycles
`1685293130`, and 953,015 worker scanlines. The inline leaf handles 79,003,085
cached fetches in that replay. The unchanged PPU/snapshot fixture evidence from
V45 is retained; the full new replay also checks every output pixel and margin.

The candidate is `output/GBAFeather-Sonic-v46-USB.uf2`, SHA256
`0748f4b036a9652c2422bdc656638fa174f40bc5c92c2ddde67787c8db6386c9`.
Its 1,038 native function ranges match the final ARM ELF exactly, map fingerprint
`1213048002`. Program size is 5,009,224 bytes; main static SRAM is 246,812 bytes,
1,112 more than V45. Pre-allocation heap capacity is 261,092 bytes, not runtime
free heap. The ROM cache/PSRAM arena is unchanged. Vectors, stack bounds,
initialized SRAM, exact ROM payload, single BIOS copy and fixed map pass the
UF2 checks. Full details are in `output/performance-v46-summary.json`.

Subsequent board captures confirm operation and a constant 8,528 bytes free
heap. The user reports roughly one FPS gained. The latest 11.17 FPS capture
is a confirmed menu/transition, so a controlled gameplay improvement is still
unmeasured. Full native histograms now identify the CPU loop, bus reads and
renderer submission as major sampled locations; the loop includes inlined
fetch/ALU work. Transition misses can also produce substantial decode pauses.
See [V46 board findings](V46-BOARD-RESULTS.md) for scene labels, deduplication,
sampling limits, stutter evidence and the next same-scene fetch on/off test.
Histogram serialization overhead and peak memory usage remain unisolated.
The modified core, supplied assets and game-containing firmware remain private;
publication includes only original support tools and evidence.
