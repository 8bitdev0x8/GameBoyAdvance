# Research assessment: CPU execution and the 60 FPS target

Date: 26 October 2026. This is a source review and research assessment, not a new firmware performance result.

## Current evidence

The corrected Casper direct-Flash build measured12.04845 FPS versus a fresh10.49835 FPS baseline (+14.7651%). Exact benchmark CPU/RAM/video endpoints and guest cycles match. Run78.32267 ms/frame, previously90.16669. Approximate total measured frame time83.0ms, compared with16.67ms for60FPS. Thus the measured window needs about4.98x throughput. At240MHz the60FPS frame budget is4 million host cycles; current Run alone consumes about18.8million elapsed core0 cycles, including interruptions/stalls. These are not retired-instruction counts.

Similar FPS across games does not isolate a cause. Memory stalls, interrupt work, dispatch, event accounting and execution all contribute to elapsed CPU time. Host cycles per guest cycle is not host cycles per guest instruction: instructions have different timings and DMA/HALT can advance guest time.

## What is already implemented

The normal emulator is event-scheduled. Emulator::run_frame calls cpu_run_until(next_event), then services hardware. The CPU executor also batches timer/IRQ housekeeping every16 instructions. It retains per-instruction prefetch, event/DMA, halt and interrupt-related work. There are specialized instruction fetches, immutable instruction windows, a small decoded Thumb ALU-block cache and conservative polling accelerators. A proposal to merely add an event loop or predecoding duplicates existing work.

## Largest justified experiment

Build an event-bounded block executor that amortizes repeated work inside verified basic blocks while keeping guest registers/flags in host locals across multiple operations. Expand beyond the current tiny ALU groups only when a block's side effects are covered. Conservatively exit for event deadlines, MMIO, newly scheduled DMA/timer work, CPSR/IRQ changes, halt/exception transitions, uncertain accesses and writes that invalidate cached RAM code. Preserve fetch/prefetch and guest-cycle effects. Memory operations, not only explicit I/O writes, can cross event boundaries; a blanket removal of checks is not exact.

First collect guest opcode classes, block lengths/exits, bus calls and cycle-accounting counts. Compare lightly sampled/instrumented builds against the same uninstrumented fixed workload. DWT CYCCNT measures elapsed cycles; it does not by itself classify them into decode, memory or Flash versus PSRAM. Optional DWT stall/exception counters and source-level measurements need support checks, overflow handling and overhead accounting. Nested measurements must not be added as independent costs.

## Research findings

- The mattdeeds Pokémon Emerald port reports60FPS in most scenes, excluding a costly intro. It rebuilds a decompiled game as native M33 software, with game-specific hardware substitutions, a second-core software PPU, and incomplete audio/peripheral features. It demonstrates native game-port feasibility, not60FPS generic accurate GBA binary emulation. Its reference board has16MiB flash and an11.7MiB image, beyond this Feather's8MiB flash capacity.
- Its display driver uses a DMA control-block ring so software is not responsible for rearming every scanline. This is a useful separate experiment to revisit, not a demonstrated drop-in fix: this project's earlier ring attempt produced corruption. Preserve the working driver as fallback, verify phase/DMA ownership and RP2350-E5 abort ordering, and measure the actual IRQ reduction before claiming a gain.
- JRickey/gba-recomp provides a static translation route. This project already ported an alternative native path to the board; claims that no MCU experiment exists are outdated for this project. It has not beaten the working interpreter. Block chaining, fewer runtime callbacks, live-register retention and executable placement need measurement; whole-ROM translation is not automatic optimization.
- Reusing an unchanged arbitrary GBA ROM as a native M33 program is not viable. ARM-mode execution, Thumb semantics, memory-map/MMIO access, exceptions and GBA hardware timing need translation or emulation. Game source recompilation, binary translation and direct execution are distinct approaches.

## Other experiments and limits

Separate experiments: autonomous HSTX scanout; SRAM instruction-line cache versus Flash pointers; event-aware direct memory paths; cheaper dispatch/predecoded blocks; codegen/register-allocation review; renderer tile-span/palette/window fast paths guarded by pixel tests. Lazy flags, computed goto and inlining are hypotheses, not guaranteed gains. General EWRAM-in-SRAM must pass a complete memory budget, including the150KiB scanout buffer, core stacks, renderer/runtime state and hot code. The+6.9% IWRAM/VRAM placement result was in the separate headless native runtime and cannot be applied to the current interpreter as a measured gain.

Renderer draw time around15.7ms in the earlier Casper comparison is an average of overlapping work, not proof of a universal hard FPS ceiling. It leaves little margin near60FPS. Dirty-line reuse requires complete dependencies, including palette, OAM, scroll/affine/window/blend state and raster-time writes. Overclocking240 to300MHz offers at most1.25x ideal compute throughput, not the5x required; no clock/voltage change is proposed here. The estimated1.5-2x interpreter and25-40FPS outcomes in the pasted advice are unmeasured predictions.

## Primary sources checked

- https://github.com/mattdeeds/pokeemerald-rp2350
- https://github.com/mattdeeds/pokeemerald-rp2350/blob/master/docs/PORTING.md
- https://github.com/mattdeeds/pokeemerald-rp2350/blob/master/docs/HARDWARE.md
- https://github.com/JRickey/gba-recomp
- https://www.raspberrypi.com/products/rp2350/
- https://www.raspberrypi.com/documentation/pico-sdk/hardware.html
- Arm Cortex-M33 TRM, DWT functional description: https://documentation-service.arm.com/static/5f15c42420b7cf4bc5247f3a

No claim that generic60FPS on this Feather is impossible or established follows from these sources. The next decision should be based on matched board measurements of an event-bounded executor, with original timing/state/pixel validation retained.
