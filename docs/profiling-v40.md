# v40: measure the work running on core 1

V39 now boots and runs on the Feather. The reported 5–12 FPS is a range,
not a sustained average. Its matched sample 314 has Wait 8 / Qfull 8 /
Barrier 0 ms/frame: the reported waiting is queue-capacity pressure in that
interval. The earlier Draw 27 / Wait 25 sample 176 is a different interval.

Core-0 PC samples identify interpreter dispatch, instruction fetching and
scanline submission. Submission includes waiting for queue capacity. Those
samples cannot tell us which drawing operations consume core 1's time.
V40 adds elapsed-time measurements inside the existing renderer to answer
that question before selecting a rendering optimization.

## Flash and report

Use `output/GBAFeather-Sonic-v40.uf2`. The startup banner is
`BOOT v40 - SONIC / CORE 1 TIMINGS`. The unmodified, board-confirmed v39
image remains `output/GBAFeather-Sonic-v39.uf2` for manual fallback; v36
is also retained. ROM, BIOS selection, clocks and UART settings are retained.

Every five seconds the diagnostic text refreshes. Renderer and existing
core-0 pages alternate, so a renderer page appears every ten seconds and
stays visible for five seconds. Game pixels and input continue at their
normal frame/slice cadence. All renderer rows and the main Run/Draw row
share one `#nnn` sample identifier and averaging interval.

Read these together on the same slow Sonic screen:

```text
v40 ... FPS | PAD ... | VBskip ...
Run ... Draw ... Copy ... Wait ... #nnn
C1 BG... OBJ... Mix... #nnn
Prep... RGB... Other... #nnn
W... Q... B... ms/f #nnn
```

Except FPS and VBskip, the values above are milliseconds per emulated
frame. Detailed renderer/wait values use tenths of a millisecond; the
existing Copy/Wait fields in the top timing row retain integer display.
Report FPS, Run, Draw, BG, OBJ, Mix, Prep, RGB, Other and W/Q/B with the
matching sample number. Record whether startup says `BIOS SRAM COPY` or
`BIOS FLASH VIEW` when comparing runs. Compare the same scene over several
intervals; a menu and gameplay can have very different workloads.

## What the measurements include

| Field | Measured region |
| --- | --- |
| Prep | Scanline setup, output selection, scratch-buffer initialization and forced-blank fill |
| BG | Text, affine or bitmap background generation |
| OBJ | Sprite rendering, including sprite-cache rebuilding when needed |
| Mix | Layer ordering, windows, transparency, blending and final RGB555 pixel composition |
| RGB | The existing RGB555-to-RGB565 conversion and strided HDMI-buffer writes |
| Other | Snapshot application, dirty checks and remaining work inside the worker's existing Draw measurement |
| W | Core-0 time waiting for the renderer |
| Q | The portion of W waiting for a full four-job queue |
| B | The portion of W spent draining at explicit barriers |

Raw Prep + BG + OBJ + Mix + RGB + Other equals the worker's Draw counter.
Independently rounded displayed numbers may differ slightly. W = Q + B
before rounding. **Draw and Wait must not be added to Run**: Run includes
waiting, while core-1 drawing overlaps core-0 execution. These are wall-time
measurements, not measurements of CPU utilization or isolated memory latency.
IRQ/preemption or contention during a measured region belongs to that region.

Timing calls perturb execution and can affect FPS. This is a diagnostic
build, with no claimed speedup. Core-0 PC sampling remains enabled; its
sampling window is separate from the numbered renderer interval.

## Counter ownership and memory

Only the consumer on core 1 updates the stage counters. Core 0 reads them
after the existing frame drain acquires completion; no shared live polling,
additional per-scanline locks or dynamic allocations are introduced.
Register snapshots and the four-job queue are unchanged. Dirty/clean job
counts are retained for validation. Unsigned counters wrap modulo 2^32;
five-second snapshots use subtraction to obtain interval values.

The clock is the RP2350 hardware microsecond timer. Tests use the host's
monotonic microsecond clock. Raw interval sums are checked before display;
`C1 timing invalid` indicates inconsistent accounting rather than a valid
profile. Counter accumulation/publication after the existing Draw end
timestamp is outside Draw, as is core-0 formatting of the HUD.

## IFetch and fills

IFetch means fetching the next emulated GBA instruction. Eligible ROM code
reads use a dedicated 256-byte SRAM window. `IFetch hit 96%` describes reuse
of that window, not all guest instruction reads, all memory accesses or
overall CPU efficiency. RAM/BIOS instructions and excluded cartridge paths
are outside this ratio.

`fills/s` counts attempts to refill that window each second, usually when
execution crosses a 256-byte boundary or branches to another window. A
refill copies 256 bytes from the ROM cache into the dedicated instruction
buffer. It may hit the SRAM ROM data cache, fetch a line from an already
decompressed PSRAM block, or require a ROM-block decompression. Consequently
fills are **not** decompression counts. `ROM miss/s` and `decode ms/s`
describe the backing-block cache/decompression separately. Thousands of
fills can coexist with a high hit rate when there are many fetches.

## Validation and limits

* Counter tests cover wrap subtraction, invalid partitions, zero-frame
  denominators, wide arithmetic, exact row units/sample IDs and bounded text.
* Profiling-enabled rendering matches the independent pre-optimization PPU
  across 240 randomized fixtures, all six graphics modes, windows/effects,
  repeated frames, memory changes, full/cropped output and RGB565 margins.
* 38,400 queued scanlines with concurrent live register writes match that
  independent PPU; raw timing partitions and job accounting pass.
* The 6,000-frame Sonic test enables profiling, threaded rendering, sliced
  execution, scripted input, direct RGB565 output, muted synthesis and the
  borrowed original BIOS. CPU/banked state, cycles, bus state, RAM and every
  pixel/margin match the conservative reference, including display replacement
  at frame 3000. All 20 checkpoints match v39/v36. Every frame validates the
  stage partition against Draw and completed-job counts.
* Existing CPU, memory, BIOS, timer, DMA and bounded-scan audits pass. Final
  UF2/ROM/BIOS/map/SRAM/stack checks are recorded in the private output summary.

Board startup, stage timings, instrumentation overhead and FPS for v40 still
require physical testing. Host parity is regression evidence, not proof of
original-console accuracy or full gameplay compatibility. There is no JIT,
ROM recompilation, new graphics algorithm, extra frame skipping or hardware
GPU path in this version. Modified core/ROM/BIOS/firmware artifacts stay
private under the existing publication decision.
