# V47: CPU memory paths and 32-bit flag updates

This is a CPU-focused board trial, following the full profiles in
[V46 board findings](V46-BOARD-RESULTS.md). It retains Sonic Advance 3,
240 MHz CPU, 30 MHz PSRAM, ESP32 controller UART and the current HSTX renderer.
**A board FPS gain and the 60 FPS gameplay target are not established.**

Subsequent testing now confirms boot and gameplay: the latest user-labelled
gameplay capture is **7.59 FPS**, Run 125.02 / Draw 35.43 / Wait 14.16 ms/frame,
with a constant 8,240 bytes free heap. This does not establish a same-scene
speedup. [V47 board analysis](V47-BOARD-RESULTS.md) records the full CPU profiles,
remaining 110.85 ms CPU-side budget outside waits, and the execution-block plan.

## What changed

CPU byte, halfword and word reads from EWRAM/IWRAM can now use fixed-width
leaves. They avoid the generic bus dispatch and result-structure return while
preserving RAM mirrors, aligned bus values, guest cycles and open-bus latches.
CPU helpers still apply unaligned rotations and the original next-fetch rule
when prefetch is disabled. MMIO, video RAM, cartridge, BIOS data, DMA and other
cases retain the full bus path. No new RAM cache or large allocation is added;
PSRAM-backed EWRAM remains PSRAM-backed.

Instruction fetches inside the available BIOS image can avoid the generic
bus route. The leaf still updates the pipeline-ahead BIOS protection latch,
including the final-word behavior. Protected/DMA accesses, unsupported access
flags and requests beyond the supplied image retain the previous path.

Arithmetic flag updates pack NZCV into one CPSR write. ADC/SBC use equivalent
32-bit carry/borrow and overflow expressions instead of signed/unsigned 64-bit
range arithmetic. Ordinary addition derives carry from its wrapped low result,
allowing the compiler to discard unused high-word work. Shift/MOV updates
preserve C/V, and every update preserves the remaining CPSR bits. This changes
interpreter work, not the guest instruction count, event schedule or clock.

## Runtime comparison and fallback

The new RAM/BIOS paths default on. USB can change both at a completed-frame
boundary; status replies `MEM 0` or `MEM 1`. Mode changes reset frame timing
baselines and mark the crossing CPU window contaminated. Frame flag 256 is
decoded as `fast_memory_on` by the updated tool.

```powershell
python GBAPrototype/tools/block_control.py --port COM9 --engine memory --mode status
python GBAPrototype/tools/block_control.py --port COM9 --engine memory --mode off
python GBAPrototype/tools/block_control.py --port COM9 --engine memory --mode on
```

Memory off restores the V46 RAM-read and BIOS-fetch routes; packed flags stay
active. Cached ROM instruction fetching remains on; blocks and ALU burst stay
off. The unchanged **V46 UF2 is the full manual fallback**, including its older
flag implementation. No experimental block or translation engine is enabled.

## Controlled host results

Four rotating-order repeats use one pinned logical CPU, HIGH priority and
MSVC /O2. Guest cycles, PC and CPU/video checksums match for each workload.
The baseline uses V46-style reads and legacy flag calculations. The final
candidate enables packed flags and both memory paths.

| Warmup / measured frames | Workload | Candidate elapsed change |
| --- | --- | ---: |
| 3,600 / 2,400 | CPU only | 0.57% more |
| 3,600 / 2,400 | CPU + inline rendering | 1.81% more |
| 12,000 / 600 | Later inline-render sequence | 2.29% less |

The packed-flags build with memory paths runtime off is also neutral/slower
on the host (0.36%, 2.28% and 1.54% more elapsed time). An earlier separate
48-run RAM/BIOS-only experiment is likewise mixed: its combined path costs
2.10% and 1.11% more in the first two workloads and 2.77% less in the later
one. These results do not show a dependable host gain.

The x86 host has native 64-bit arithmetic and a different calling convention,
cache and memory system from the M33. That difference justifies testing this
bounded candidate on the board; it is not evidence of a board speedup. The
replays reach entry/menu states and do not reproduce the seven-FPS gameplay
scene. Rendering source is unchanged; differences in measured host Draw cannot
be attributed to a new renderer optimization. Raw comparisons are in
`output/v47-host-benchmark.json` and `output/v47-memory-host-benchmark.json`.

## Validation

The core audit passes 16 groups, including:

- 199,200 actual ARM/Thumb ADD/SUB/ADC/SBC/CMP/CMN instructions against an
  independent signed/unsigned 64-bit flags oracle, all NZCV inputs, edge values
  and random operands; other CPSR bits and source registers are checked.
- 180,000 bus RAM reads and 192,000 BIOS fetch fixtures, including mirrors,
  all widths/alignments, partial images, pipeline tails, protected/DMA accesses
  and side-effect-free fallback checks. Actual leaf hits: 120,583 RAM / 25,386 BIOS.
- 12,000 actual ARM/Thumb RAM loads with the new paths on/off, checking CPU
  state, unaligned rotations, exact guest cycles, fetch sequencing and bus state.
- Existing exhaustive Thumb ALU, instruction-window, burst/block, video-write,
  DMA, timer and color-math audits.

The 6,000-frame Sonic replay compares the new reads with an original-ROM
reference using the old memory routes. All twenty V46 CPU/video/key-read
checkpoints match; final PC is `080bc248`, framebuffer hash `5ba30470`, guest
cycles `1685293130`, and worker scanlines `953015`. Per-frame pixel/margin,
display-target replacement, timing partitions and binary record accounting
pass. The unchanged PPU retains its 480 reference fixtures and 38,400 queued
snapshot checks; every frame's pixels are also checked in the new full replay.

Fourteen Python protocol tests pass, including the new memory mode flag and
strict `MEM` reply parser. Final UF2 checks validate vectors, stacks, initialized
SRAM, the exact packed ROM, one private BIOS copy and the exact native map.
The renderer, DMA, timer, APU, ROM cache, HSTX recovery and linker source remain
identical to V46.

The ARM build reports **5,009,512 program bytes and 247,092 static SRAM bytes**,
280 more static bytes than V46. The main/core-1 stack reservations remain
16 KiB / 8 KiB. The individual CPU-loop frame is 96 bytes; the disabled block
executor frame is 112 bytes. These are individual frames, not peak stack usage.
There are 1,036 native function ranges matching the final ELF exactly. Final
checksums, class sizes and allocation bounds are in
`output/performance-v47-summary.json`. Subsequent board logs measure 8,240 bytes
free heap; peak usage remains unmeasured.
The packaged UF2 SHA256 is
`4d1f2fe7c26e9e8d97785dc92e0137d4fe2645e71387a36e00a6b47a45636cac`;
map fingerprint is `3026315552`. The emulator object remains 86,920 bytes and
pre-allocation heap capacity is 260,804 bytes; that is not runtime free heap.

## Board test

Flash `output/GBAFeather-Sonic-v47-USB.uf2`. On an identifiable, repeatable slow
**gameplay** scene, warm for at least 20 seconds and capture:

```powershell
python GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v47-gameplay-memory-on --open
```

Use `--engine memory --mode off`, return to the same scene, repeat the same
warmup and capture `--output sonic-v47-gameplay-memory-off`. Then restore on
and repeat. Keep menu/transition recordings separately labelled. Full native
histograms, timing partitions and mode flags should be compared together.

A 60 Hz monitor signal remains independent of guest FPS. This is a correctness-
checked performance experiment, with small/mixed host results and no promised
large FPS gain. Translation, guest-PC/region attribution, PSRAM stall profiling
and renderer memory ownership remain separate work. The modified core, private
game/BIOS assets and game-containing firmware remain local.
