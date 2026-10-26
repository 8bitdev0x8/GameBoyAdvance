# v32: BIOS reads and display-register synchronization

**Subsequent board result:** the user reports no performance improvement.
Newer samples show submit_hook 22%, Bus code fetch 11%, CPU Thumb 7%, CPU
dispatch 8%, Bad0 and 36 unmapped samples. New FPS/Run/Draw/Wait values and
total sample N were not provided. See [PERFORMANCE-v33.md](PERFORMANCE-v33.md)
for the resulting instruction-fetch experiment. V32 remains bootable fallback.

## First on-board PC samples

The user supplied these v31 core-0 results:

| Window/report | Function or diagnostic | Value |
| --- | --- | ---: |
| First report | Bus read | 17% |
| First report | CPU ARM | 12% |
| First report | Cartridge read_bios | 12% |
| First report | N / Bad / unmapped | 2561 / 0 / 0 |
| First report | displayed handler cost | 0 ms/s |
| Other report | ScanlineWorker::submit_hook | 29% |
| Other report | Bus read | 12% |
| Other report | Render wait | 18% |

Scene, duration and alignment of the later windows were not supplied. Do not
add these into one distribution. Cost is rounded down: zero displayed ms/s
does not mean zero sampler overhead. No rejected or unmapped samples were
reported in the supplied diagnostic window, but interrupt-delivery biases
remain as documented in [PROFILING-v31.md](PROFILING-v31.md).

Assembly confirms that `submit_hook` includes an inlined `wait()` with WFE.
Its 29% can include waiting for core 1, rather than equivalent CPU time doing
queue bookkeeping. The existing direct-video worker remains in use and is
not replaced by an unsafe asynchronous queue.

## Changes driven by those readings

**BIOS scalar reads.** The old flash-resident `Cartridge::read_bios` delegated
to a generic byte reader. Its compiled word path included repeated bounds
tests and four byte loads, and a BIOS instruction fetch can call it again to
update the protected BIOS latch. The new inline helper bounds-checks a whole
aligned in-range halfword/word and uses a scalar load from existing SRAM BIOS
storage. The BIOS span accessor is also inline. Bus protection, pipeline-latch
selection, access cycles, BIOS contents and dispatch remain unchanged. The
partial/out-of-range halfword path retains the old helper, including its
historical 32-bit wrapped-address behavior. The focused test caught this
edge case during development; it was preserved before delivering firmware.

**Unchanged display registers.** A read-only preflight compares pure storage
registers against the effective write value before joining the renderer.
Exact unchanged assignments need neither mutation nor a join. Raw masked
register bits, byte merges and affine halves follow the original rules;
DISPSTAT/VCOUNT, unknown and partly unknown words remain conservative. Changed
writes still drain the worker before mutation. The rules were previously
audited in v29, but both PPU helpers remain in flash in this build. V29's
SRAM placements, ROM-word reuse and Exec/Event timing are not restored.

CPU instruction semantics, CPU/PSRAM clocks, memory allocations, ROM data/cache,
rendering, worker, timers/DMA/APU, HDMI driver, UART and stack layout are
unchanged. The supplied original BIOS stays selected, and the on-board PC
sampler stays enabled with a newly verified map.

## Validation

* 600120 direct BIOS reads match the preserved bounded-byte reader, across
  empty, partial, full and clipped images, byte/half/word alignment, ends,
  random out-of-range addresses and 32-bit wrap boundaries.
* 120000 BIOS bus reads match code/data/DMA/protected results, cycles and
  bus/BIOS/prefetch latches.
* 24030 display-register cases check raw/masked values, byte merges, affine
  halves, unknown/status/partial writes, readbacks, cycles and barrier counts.
* Existing 45003 video-memory cases, 600 DMA cases, 8160 Thumb cases, 30000
  instruction-fetch cases, 60000 independent timer comparisons and 240 PPU
  fixtures against the preserved v21 renderer pass.
* The final 6000-frame Sonic run uses the supplied BIOS on both paths. A
  conservative synchronous original-ROM/reference-BIOS reader matches the
  optimized compressed-ROM/threaded/direct-RGB565 path in CPU/banked/flags,
  exact cycles, bus state, EWRAM/IWRAM and every pixel/margin. Display-target
  replacement at frame 3000 passes. All 20 checkpoints match v30.

The run bypasses **1026873 display-register half assignments before the join**.
It still completes the same 953015 scanlines; this does not eliminate the
renderer's work or guarantee that total waiting falls. Final PC `080bc248`,
hash `5ba30470`, guest cycles 1685293130 and input reads 11737 match v30.

An additional native guest instruction sampler records 439912 samples over
the scripted sequence; the 16-byte Thumb block at `080bc1e0` receives 21.21%.
This is a distribution of executed guest instructions, not physical-board
wall time, and the scripted scene need not match the user's board scene. It
is not used as proof that broader idle skipping is safe.

The final board build reports 230004 static SRAM bytes: 384 more than v31,
with 277892 bytes of heap capacity before dynamic allocations. BIOS accessor
symbols/calls disappear from firmware; the new PPU preflight and existing
register reader remain in flash. UF2, exact ROM/BIOS, initialized SRAM, stacks,
PSRAM bounds and final sampling-map checks are recorded in
`output/performance-v32-summary.json`.

Native tests are regressions, not independent GBA accuracy or full gameplay
validation. No physical-board FPS improvement is measured yet. After inlining,
BIOS work can be sampled under Bus read; disappearance of a function label is
not itself a measured speedup.

## Board comparison and fallback

Flash `output/GBAFeather-Sonic-v32.uf2` with BOOT + RESET. Banner:
`BOOT v32 - SONIC / BIOS AND JOINS`. Compare FPS, Run, Draw, Wait, HDMI handler
ms/s and PC0 top functions on the same Sonic screen for at least 15 seconds.
The sampler's three labels and diagnostics retain the v31 rotation.

The confirmed v31 sampler UF2 remains unchanged as the primary manual-reflash
fallback: `output/GBAFeather-Sonic-v31-Profile.uf2`, SHA256
`20bc9eb2fb8e8b8584d93e57ae4fc1e2e6a25c7944b06098b975fbb96fb532ea`.
The working v30 original-BIOS image and v28 also remain available. V29's fault
cause remains unresolved. The current core, ROM, BIOS and firmware stay private;
no public push is part of this build.
