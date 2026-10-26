# Architecture research: a route beyond the interpreter

Research date: 20 October 2026. This is an engineering assessment, not a
measured performance improvement or a replacement firmware release.

The most promising large change is a **hybrid ahead-of-time execution backend**:
translate selected hot guest blocks on the computer, compile them for Cortex-M33
into the firmware, and keep the interpreter for everything else. This could
remove runtime compilation churn and much instruction decoding. It will only
help substantially if memory, flags and guest timing also become inexpensive.
Existing evidence does not establish that this Feather can run generic GBA
games at full speed.

## What the measured problem actually is

The last V50 capture, `sonic-v50-gameplay-2026-10-20_19-57-06-195`, is confirmed
demo play. Its 267 frames average:

| Measurement | ms per emulated frame |
| --- | ---: |
| Wall | 134.03 |
| Run | 127.28 |
| Explicit renderer wait inside Run | 17.45 |
| **Run minus explicit renderer wait** | **109.83** |
| Draw on core 1, overlapping Run | 36.48 |
| BG / composition within Draw | 16.97 / 14.17 |

**109.83 ms is not pure retired CPU work.** It includes execution, bus stalls,
hardware-service work and interrupts occurring within that interval. The logs
do not yet provide an exclusive breakdown of all of these. HDMI's separately
timed handler consumes 147.2 ms per second, about 19.7 ms across a mean whole
frame. That cannot simply be subtracted from Run-minus-Wait: the handler can
also interrupt a wait or other parts of the frame. Draw and Run overlap.

The CPU-side target of 16.7 ms needs a 6.58x reduction in this elapsed bucket,
or removal of 84.8% of it. As an optimistic Amdahl model, if fraction `p` of
the bucket is accelerated by `s`, the remainder is:

`109.83 * ((1 - p) + p / s)` ms.

| Assumed accelerated fraction | Speed of that fraction | Remaining bucket |
| --- | ---: | ---: |
| 80% | 10x | 30.75 ms |
| 90% | 10x | 20.87 ms |
| 95% | 10x | 15.93 ms |

These are hypothetical bounds, not forecasts. They explain why optimizing
one dispatch function or increasing a code cache cannot justify a 60-FPS claim.
Even a 16.7-ms CPU result would leave the 36.5-ms renderer to address.

Local evidence: [V51 analysis](PERFORMANCE-v51.md),
`output/log-analysis-v50.json` and `output/native-ab-v49-analysis.json`.

## Hardware differences that affect the design

| GBA | Feather implementation | Consequence |
| --- | --- | --- |
| ARM7TDMI, about 16.78 MHz, ARM and Thumb execution | Cortex-M33 at the project's 240 MHz setting | Guest instructions need translation or interpretation; the frequency ratio is not an emulation speed guarantee. |
| Dedicated tile/sprite/effects hardware | Software PPU on core 1 | Fast CPU execution still requires a fast renderer. |
| 32-KiB IWRAM, 256-KiB EWRAM, 96-KiB VRAM, separate palette/OAM | IWRAM/VRAM/palette/OAM already internal SRAM; EWRAM external PSRAM | Moving IWRAM or VRAM to SRAM is already done. EWRAM locality remains worth measuring. |

GBA layout and capabilities: [Tonc hardware reference](https://gbadev.net/tonc/hardware.html).
ARM execution and timing: [Arm ARM7TDMI technical reference manual](https://documentation-service.arm.com/static/5f4786a179ff4c392c0ff819).
The [M33 user guide](https://documentation-service.arm.com/static/66be2153882fec713ef49e49)
specifies Thumb-only execution. Shared instruction ancestry does not provide
the GBA's memory map, privileged modes, timing, or peripherals. ARM-mode code
must be converted; Thumb sequences still need address/PC/state handling.

The RP2350 has ten SRAM banks, with two 256-KiB striped groups and two 4-KiB
banks. Its shared XIP cache is **16 KiB**; flash and PSRAM use QMI. The chip
includes DSP operations, interpolators and HSTX encoding, but no GBA PPU.
Cache pinning consumes existing cache capacity rather than adding RAM.
These details come from the [official RP2350 datasheet](https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008373-DS-2-rp2350-datasheet.pdf?disposition=inline),
sections 3.7.2, 4.2 and 4.4. The accessible edition consulted is build
2025-07-29, `d126e9e-clean`.

Project implications, which remain hypotheses: partition high-bandwidth code,
stacks and scanout buffers only after measuring contention; keep hot translated
code small enough to remain resident; benchmark DSP/interpolator setup costs
against ordinary integer span loops. FPU work is a low priority for an integer
guest CPU. A new PSRAM code tier needs an explicit arena rebudget: the current
7.5-MiB arena and ROM backing already consume most external RAM.

## Why the current translator was slower

The controlled V49 A/B/A board observation measured interpreter/native/
interpreter at 7.51 / 5.31 / 6.87 FPS. Native mode performed approximately
2,034 compilations per second and averaged **3.36 guest instructions per
native invocation**. Rendering cost stayed similar. The trial had zero input
and some scene drift, but native was slower than both interpreter observations.

Inspection of our implementation explains plausible sources of that overhead:

- The executable pool is only 2,304 bytes with 16 descriptors. Entries are
  replaced as variable-size emitted blocks overlap in the ring.
- Each block contains at most 16 guest instructions and covers only selected
  ROM Thumb instructions. ARM, BIOS and IWRAM execution fall back.
- Timing gates retain per-fetch state transitions and repeated deadline checks.
  Memory operations still call the C++ bus helpers.
- Registers survive calls, but call setup, guest flag synchronization, fetch
  bookkeeping, lookup and compilation are still real work.

Simply enlarging the pool would attack churn but would not remove all these
costs. Native mode should remain off by default until a replacement wins a
matched board comparison. V51's interpreter specialization saves about 4%
in local tests; it is not the required architectural speedup.

## Execution design worth prototyping

1. **Measure guest coverage.** Count guest instructions and branch targets by
   ARM/Thumb mode and BIOS/ROM/IWRAM/EWRAM region; record frequent block entries,
   SWI calls, helper calls and event exits. Our current PC sampler profiles
   *host functions*, so it cannot establish which guest routines dominate.
   Start with a deterministic host run and validate the ranking on the board.
2. **Translate a small hot immutable-ROM set on the PC.** Generate M33 code or
   C/C++ for existing hot blocks, bind it to the ROM hash, and link it into flash.
   The uploader already rebuilds for a selected ROM, so this could become a
   build stage. Unknown targets, unsupported instructions and mutable RAM code
   keep the interpreter. Translation of a few hot blocks is an experiment, not
   a claim that arbitrary ROM discovery is solved.
3. **Keep state live across connected blocks.** Avoid saving/restoring every
   guest register and returning through the C++ dispatcher for every short
   sequence. Use bounded execution and exact exit state so the existing core
   can resume. Include ARM blocks if the measured workload requires them.
4. **Inline guarded ordinary-memory operations.** Compute direct host addresses
   for ordinary RAM and preserve alignment, rotation, mirroring, wait-state
   and open-bus behavior. MMIO, special cartridge accesses, video writes and
   ambiguous cases use audited helpers. EWRAM remains physically PSRAM unless
   a separate locality experiment justifies reallocating SRAM.
5. **Reduce timing overhead at proven-safe boundaries.** Use a local cycle
   budget and fold fixed costs over side-effect-free sequences. Stop before
   events, MMIO or potentially relevant writes; preserve fetch/prefetch state,
   IRQ ordering and DMA interaction. Initially reject a block near a deadline
   and interpret its tail. Do not skip hardware checks indiscriminately.
6. **Prove value before expanding.** Compare the exact same block with the
   interpreter using randomized registers/flags, memory and event positions;
   then replay full frames. Count actual M33 work, but make real-board elapsed
   time the acceptance criterion. Record compiled size and hot-set coverage.

The [mGBA ARM loop](https://github.com/mgba-emu/mgba/blob/master/src/arm/arm.c)
and [ARM state definitions](https://github.com/mgba-emu/mgba/blob/master/include/mgba/internal/arm/arm.h)
are useful references: direct active-region opcode access, cached access costs
and a next-event-driven loop. Our core already has fixed-width fetch leaves
and a scheduler, so copying this idea alone is not new work. The research
question is how much repeated bookkeeping can safely leave the hot path.

### A useful newly found resource: GBARecomp

[mstan/gbarecomp](https://github.com/mstan/gbarecomp) describes an experimental
ARM/Thumb-to-C++ static recompiler with an interpreter fallback. It is a useful
source of discovery, intermediate-representation and fallback design ideas.
It is not an established RP2350 solution or a measured speed result for Sonic.

Its [runtime interface](https://github.com/mstan/gbarecomp/blob/main/src/armv4t/runtime_arm.h)
still exposes bus, flags and per-instruction timing helpers. Its
[build](https://github.com/mstan/gbarecomp/blob/main/CMakeLists.txt) also supports
desktop runtime compilation into a DLL. That compiler-on-the-device path is
unsuitable for this MCU. Any evaluation here would use the PC-side translator
and a small embedded runtime, with a measured flash/RAM budget. Ahead-of-time
code can still be too large or helper-heavy; the name “native” proves neither
correctness nor speed.

## Hardware measurements that are still missing

The [Pico SDK BUSCTRL register definitions](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2350/hardware_regs/include/hardware/regs/busctrl.h)
document four 24-bit saturating counters. Select XIP and SRAM events in short
windows, collecting accesses, contested accesses, upstream stalls and downstream
stalls. They observe shared downstream ports, not exclusive per-core CPU time;
do not turn their sum into a “core utilization” percentage. Flag saturation
and keep windows short enough to remain valid.

Also capture XIP access/hit deltas and per-core elapsed cycle counts alongside
guest instructions, IRQ time and frame timings. Interpret misses with care:
XIP access counts include uncached accesses. These measurements should distinguish
expensive software instruction handling from a memory-latency limit. The old
dependent-access probe (S32 21 ns, P32 767 ns, P256 1,188 ns) shows that tested
external access patterns are expensive, but it does not measure the fraction
of gameplay spent waiting for them.

## Scanout and rendering remain separate work

The [official DMA control-block example](https://github.com/raspberrypi/pico-examples/blob/master/dma/control_blocks/control_blocks.c)
and [HSTX encoder example](https://github.com/raspberrypi/pico-examples/blob/master/hstx/dvi_out_hstx_encoder/dvi_out_hstx_encoder.c)
show building blocks for scanout offload. A control-block stream is worth
testing against the current scanline-interrupt driver. Removing its full
measured 14.7% elapsed cost would yield only about a 1.17x ideal improvement
if nothing else changed; this cannot explain a 6.6x CPU-side solution.

The [pokeemerald RP2350 porting log](https://github.com/mattdeeds/pokeemerald-rp2350/blob/master/docs/PORTING.md)
reports native game logic, tile-span rendering, uniform-window/no-op-effect
fast paths and CPU-independent scanout. The renderer techniques are relevant,
but the project's native source port avoids guest CPU interpretation and is
not evidence of generic ROM emulation at 60 FPS. Our next renderer target is
the BG/composition pair, about 85% of measured Draw, with snapshot pixel tests.

## Decision

Proceed toward a measured hybrid execution prototype and hardware stall
profiling. Preserve the working interpreter, compression, input and video
fallbacks. Use the [mGBA hardware test suite](https://github.com/mgba-emu/suite)
as an additional independent check before trusting timing/CPU changes; the
existing same-core differential tests cannot prove every inherited behavior.

Success is a repeatable reduction in matched board Run-minus-Wait and whole
frame time, with correct execution and bounded memory. A 6.6x CPU-side gain,
full-speed rendering and generic 60-FPS gameplay are still unproven.
