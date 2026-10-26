# v39: safe BIOS storage after the v38 allocation abort

## Subsequent board measurements

V39 reaches runtime and the user reports a 5–12 FPS range. The matched row
at sample 314 reads Wait 8 / Qfull 8 / Barrier 0 ms/frame. Waiting is therefore
queue capacity in that interval; explicit drain time rounds to zero. Separate
renderer video memory is not the immediate priority for this workload.

An earlier report reads Draw 27, Wait 25 at sample 176, and separately Qfull
29 without a sample ID. Those values cannot be treated as one matching row.
Other reported readings: CPU dispatch 20%, Bus code fetch 19%, scanline
submit 15%, IFetch 96%, fills 5963, IRQ count 3149, handler 147 ms/s. The IRQ
count is preserved literally. Scene, duration, Run time and BIOS storage mode
are unspecified; no sustained average or controlled FPS gain is established.

The exact v39 map places all three sampled functions in SRAM. Scanline submit
includes its inlined wait; its percentage does not measure queue-copy work
alone. These are core-0 samples and cannot identify core-1 drawing hotspots.
IFetch measures eligible instruction-window reuse, not total memory stalls.
The measured handler interval costs about 14.7% of one second on core 0.
Further work should separate drawing stages and interpreter/fetch cost;
the earlier Draw 27 is not combined with the later Wait 8 as a single sample.

## Confirmed failure path

V38 reaches **5: LOAD LOCAL GBA BIOS** and reports SOFTWARE ABORT, CFSR/HFSR
zero, core 0, abort return `10018360`, LR `10018361`, MSP `2007FF08`.
The frozen v38 ELF shows the preceding instruction at `1001835C` calls
abort from `operator new(unsigned int)` after malloc returned null and no
new-handler was installed. `10018360` happens to be the next function's
start (`operator new[]`); resolving the return instruction is essential.

Stage 5 constructed a 16 KiB vector to copy the supplied BIOS into SRAM.
The new record therefore identifies a failed C++ allocation on that path.
It does not establish whether low total heap, fragmentation or an earlier
allocator problem caused malloc to fail. It also does not prove that the
earlier v37 scan abort had exactly the same cause.

The bounded scanner got past the previously failing stage. V38 is withdrawn
as a working image. Its original ELF remains under `build/frozen-v38/`, SHA256
`5b96158b4335df7534c2a82bc16f10c384ff6796bdbc5926bdd5e74a35556af9`.

## BIOS loading change

The frontend no longer constructs an unconditional BIOS vector. It selects
an immutable view of the exact embedded BIOS bytes:

* With reported free heap of at least BIOS size plus an 8 KiB reserve, it
  attempts one checked malloc. On success it copies the BIOS into SRAM.
* With less heap, or if that allocation returns null, it uses the existing
  flash image directly. No copy or C++ allocation is needed for fallback.

The startup line reports **BIOS SRAM COPY** or **BIOS FLASH VIEW**, followed
by free SRAM. The reserve is an allocation policy, not a proof that every
later allocation will succeed. The source flash image and any successful
copy both live until board reset.

Cartridge keeps an explicit read-only BIOS span alongside the existing owned
vector API used by other frontends/tests. Normal owned loads update the span;
external binding releases a previous owned image without allocating. Reads,
size clipping, protected BIOS latches, normal SWI execution and guest cycle
accounting are unchanged. No alternative BIOS or ROM patch is selected.

The view keeps hot BIOS reads in SRAM when there is room, while prioritizing
boot over an unchecked allocation when there is not. The performance cost
of flash fallback needs board measurement; no FPS gain is claimed.

## Validation

The storage-policy test covers low heap without an allocation attempt,
malloc returning null despite a large reported heap, success at the exact
reserve threshold, all 16 KiB of copied/fallback bytes, an empty image, and
owned-to-view-to-owned replacement. C++ new/new[] is denied while testing
fallback binding. The 260 bounded scan tests remain passing.

600120 borrowed BIOS scalar reads match the owned byte-reader reference
across empty, partial, clipped and full images, alignment and end cases;
120000 mixed protected/data/DMA/code accesses preserve results, cycles and
bus/latch state. Other CPU/video/DMA/timer audits pass.

The 6000-frame Sonic test uses a borrowed BIOS on the optimized emulator and
an independently owned BIOS on the conservative reference. CPU/banked state,
cycles, RAM, bus/latches and every RGB565 pixel/margin remain equal, including
display replacement at frame 3000. All 20 checkpoints match working v36.
Host memory cannot reproduce flash latency or prove board stability.

Final image/ROM/BIOS/map/stack/layout checks are recorded in
`output/recovery-v39-summary.json`. Renderer, CPU execution, scanout, clocks,
UART and fixed arenas remain unchanged. All modified core and firmware
artifacts stay private.

Flash `output/GBAFeather-Sonic-v39.uf2`; banner
`BOOT v39 - SONIC / SAFE BIOS LOAD`. The unchanged working v36 image is the
manual fallback. V39 startup/runtime is now confirmed; sustained performance
and the selected SRAM/flash BIOS storage mode remain unspecified.
