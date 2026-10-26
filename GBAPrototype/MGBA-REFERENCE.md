# mGBA reference review

Reference supplied by the user: https://github.com/mgba-emu . Reviewed the
project's current primary-source ARM interpreter, GBA memory and event code on
2026-10-19. This review does not replace the present core or claim that mGBA
has been ported to RP2350. No mGBA code is copied into this build.

## Concrete design differences worth testing

1. **Instruction fetch:** mGBA's `ARMStep` and `ThumbStep` fetch directly from
   an active memory-region pointer and mask. Branch/region handling establishes
   that mapping and its wait-state information. This core calls fetch wrappers,
   bus dispatch and the cartridge/ROM-cache path repeatedly. A small pinned
   instruction window could reduce those lookups. Our compressed ROM cache can
   evict a line during data reads, so retaining a pointer without a pin or
   generation check is unsafe. GPIO/EEPROM, bounds, prefetch, bus latches and
   guest timing still require their existing rules.
2. **Instruction dispatch:** mGBA uses separate Thumb/ARM handler tables and
   runs the selected instruction mode until its next event. The present core
   rechecks mode and enters its large decoders each iteration. A smaller
   table/dispatch experiment is plausible; indirect branches, flash-cache
   pressure and the Feather's SRAM budget mean it needs measurement rather
   than an assumed win.
3. **Event handling:** mGBA's timing queue invokes due callbacks and returns
   the next deadline. This core refreshes several scheduler slots and services
   hardware around execution batches. Profiles should establish the time
   spent there before redesigning it. DMA, timer/IRQ phase and silent audio
   FIFO timing must remain correct.

These are design candidates inferred from comparing the two implementations.
They are not measured speedups. The original BIOS still invokes an interpreter;
changing the BIOS does not turn guest ARM instructions into native M33 code.
The referenced mGBA path is also an interpreter, not evidence of an automatic
ROM-to-native converter. A full port would additionally need memory, compressed
ROM, timing, video and board integration work, with board benchmarks.

## Sources

* ARM run loop, active-region fetch and dispatch:
  https://github.com/mgba-emu/mgba/blob/master/src/arm/arm.c
* Thumb instruction handlers:
  https://github.com/mgba-emu/mgba/blob/master/src/arm/isa-thumb.c
* Active memory region and cached wait-state setup:
  https://github.com/mgba-emu/mgba/blob/master/src/gba/memory.c
* Due-event queue:
  https://github.com/mgba-emu/mgba/blob/master/src/core/timing.c

These URLs follow master; the observed behavior should be checked again if
implementing a later port. mGBA source headers identify MPL-2.0. Existing
project restrictions on publishing the current modified core and private
ROM/BIOS firmware still apply.
