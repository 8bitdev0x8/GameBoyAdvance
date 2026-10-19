# Video writes, memory measurement and acceleration: v28

## Hardware reports through v28

On 19 October 2026 the user confirmed **v28 at 7 FPS, PAD ON, VBskip 12%,
Run 113, Draw 22, Copy 0 and Wait 14 ms/frame**. Scene and duration were not
specified. The memory benchmark table, HDMI reset/IRQ figures and ROM miss/decode
values were not supplied for v28. These remain unknown rather than zero.

The earlier v27 report follows for context, not as a controlled same-scene test.

The user reports v27 averaging 7 FPS, PAD ON, VBskip 10%, Run 121,
Draw 33, Copy 0 and Wait 32 ms/frame; HDMI resets 5, IRQ/s 31495.
ROM misses and decode are normally zero with occasional spikes. Scene and
measurement duration were not specified. Wait is included in Run and rendering
overlaps CPU execution, so these times must not be added. The reset count is
cumulative and includes the two intentional startup resets; further increments
during gameplay still need investigation.

## Implemented

- Palette/VRAM/OAM halfword/word writes compare the effective stored value,
  including alignment, mirrors and GBA byte-write replication/ignore rules.
  Unchanged writes retain guest bus cycles and access bookkeeping but avoid
  the memory write, renderer join and dirty/cache changes. Core 0 is the sole
  writer; core 1 only reads these arrays. A changed write still joins before
  touching shared memory or PPU caches.
- Storage-only display registers compare old and new state before marking
  scanlines dirty. Their writes retain the original renderer barrier and
  assignment/merge behavior. DISPSTAT and its flag update remain unchanged.
  This optimizes this core's existing register model, not an independently
  validated implementation of every GBA affine-register latch behavior.
- Small changed VRAM ranges invalidate overlapping 4bpp/8bpp decoded-row cache
  slots. Slot aliasing also covers the renderer's wrapped addresses. All lines
  are still conservatively marked dirty on actual changes. Unknown or large
  ranges use the existing global epoch invalidation, including wrap protection.
- Fast ROM/RAM-to-VRAM DMA drains once before the burst, compares each effective
  value and records the union of changed physical bytes. It invalidates once
  after the burst. No render job can be submitted during this core-0 operation.
  Guest reads, DMA latches, addresses, guest cycles and IRQ behavior are retained.
- Generic memory helpers bypass runtime modulo for already in-range addresses.
  Out-of-range normalization and fallback behavior remain.

## What Sonic actually exercises

In the scripted 6000-frame native sequence:

| Counter | Result |
| --- | ---: |
| Video writes, including optimized DMA paths | 4,196,721 |
| Unchanged video writes | 4,141,934 (98.69%) |
| Storage display-register half writes | 1,154,176 |
| Unchanged register halves | 1,039,108 (90.03%) |
| Decoded tile-row cache misses, conservative reference | 8,011,631 |
| Decoded tile-row cache misses, optimized | 3,848,078 (51.97% fewer) |
| Worker jobs, v27 / v28 | 958,816 / 958,723 |

The almost unchanged job count is important: this does not eliminate rendering
most lines. It avoids millions of redundant writes/barriers and preserves more
decoded rows between real changes. These counts describe this language/name-entry
sequence, not every Sonic level, hardware utilization, elapsed time or FPS gain.

The normal-ROM synchronous side disables the new video optimizations for
comparison. Exact CPU/banked/flags, cycles, bus/prefetch/latches, EWRAM/IWRAM,
every RGB565 pixel and protected margin match the compressed/fast-fetch/threaded
side on all 6000 frames. All 20 checkpoints match v27, final PC `080bcca6`,
hash `5ba30470`, cycles 1685292894 and key reads 11861. Display storage is
replaced at frame 3000. The additional video tests cover 45003 writes/reads,
600 DMA cases, same-value barrier/dirty-state behavior and 240 fixtures against
the independent preserved v21 renderer, now using sparse range invalidation
and large-range fallback. CPU/fetch/timer regressions also pass.

## Measuring the real board

At startup, stage 13 measures existing IWRAM/EWRAM buffers before guest reset.
It shows a table for **SRAM 32 KiB, PSRAM 32 KiB and PSRAM 256 KiB** for 15
seconds, then starts the game. No extra PSRAM allocation is made; the normal
reset clears benchmark data before guest code executes. These loops execute
from SRAM, with HDMI enabled and core 1 not yet launched.

Each column is **ns per 32-bit operation** over 131072 operations:

- `SeqR`: sequential reads.
- `DepR`: a dependent pointer chain through a full-period permutation. Each
  address depends on the previous memory result; this exposes access latency.
- `SeqW`: sequential writes.
- `RandW`: writes scattered by the same permutation; includes index generation.

These are CPU-observed cached memory/loop costs, including HDMI interrupts and
write completion barriers. They are **not isolated raw-chip or wire latency**,
nor a measurement under simultaneous emulation/rendering. Compare equal 32 KiB
windows first. The 256 KiB window explores the full EWRAM working set. DepR
results also alternate with the key/PC HUD line during play. Values must be read
from the board; desktop tests cannot provide them. A large PSRAM/SRAM difference
establishes an access penalty, but its FPS share still depends on access counts.

The IRQ wrapper now accumulates the time inside the original video handler.
The HUD adds `N ms/s`: 100 ms/s is approximately 10% of one second in that
handler. It excludes some interrupt entry/exit and wrapper overhead, and the
profiling itself adds two timer reads per IRQ. The installed HDMI driver is
unchanged. IRQ frequency alone is not utilization. Both cores are used, but
neither core's full busy/stall/idle utilization has been established.

## Hardware acceleration candidates

The [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
and [Pico SDK interpolator documentation](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#group_hardware_interp)
describe the useful blocks. Their relevance here is:

| Block | Present use / possible next work |
| --- | --- |
| HSTX encoder and DMA | Already used for video. The installed `DVHSTX::gfx_dma_handler` still expands each changed source row from 320 RGB565 pixels to 640 output pixels in software. A different DMA/scanout arrangement might reduce this work and IRQ frequency; benchmark its CPU cost before redesigning it. |
| Per-core interpolators | Candidate for repeated address generation, affine texture lookup and selected palette/table operations. Existing affine loops already use incremental arithmetic, so benefit needs an isolated on-board comparison, not an assumption. |
| DMA bulk transfer | Candidate for sufficiently large copies/fills. Full-frame copy is already removed; setup cost and bus contention can outweigh savings for short rows or scalar guest accesses. DMA cannot perform GBA layer/window/sprite logic by itself. |

No new interpolator or DMA scanout backend is enabled in v28. There is no
general GBA CPU-execution accelerator in these blocks. PIO/HSTX/DMA are not
replacement ARM7 interpreters.

## Larger changes and remaining limits

The interpreter still repeatedly decodes and executes guest ARM7 instructions.
ROM page decompression is separate from instruction decoding; this build does
not recompile the ROM. Zero ROM misses/decode remove that particular steady-state
cost, not bus dispatch, guest instruction decode, PSRAM access or scheduling.
A decoded-instruction cache or dynamic binary translator is a larger possible
CPU improvement, requiring coherent handling of writable code, CPU state, bus
timing, interrupts and limited SRAM. No speedup or 60 FPS result is established.

UART is 86 bytes/report at approximately 100 reports/s, 8600 bytes/s. At 230400
baud, 8N1, that is about 37.3% link occupancy. The existing receiver only consumes
available bytes with a bounded budget, without waiting for a whole packet.
Parsing/CRC/interrupt cost is nonzero and has not been separately measured; there
is no evidence it accounts for the roughly 7 FPS performance. Guest input is
processed infrequently at low emulation FPS, independently of wire delivery.

Empty-list CPU transfers and ARM PC/base cases, complete gameplay accuracy,
tearing, HDMI resets, audio and persistent saves remain open. Native comparison
is regression evidence within this emulator, not a hardware conformance test.
V28 is confirmed running at approximately 7 FPS with Draw 22 and Wait 14 ms/frame.
Its memory-benchmark and IRQ-cost results have not been supplied. No controlled
same-scene measurement establishes an overall FPS speedup over v27.
Keep modified core sources and ROM-containing firmware private.

## Next profiling steps: real board hotspots

These are proposed follow-ups, not features enabled in v28:

1. **Periodic PC sampling, separately per core.** Reserve an available hardware
   timer/alarm and route its interrupt to the target core, respecting SDK alarm
   ownership. At 250-1000 Hz, a short exception-entry trampoline captures
   EXC_RETURN and the interrupted MSP/PSP before an ordinary C prologue changes
   them. Decode the correct basic/extended exception frame and its PC. Bin the
   interrupted RP2350 address in a bounded SRAM histogram. Re-arm/acknowledge
   the interrupt and return. No allocation, file output, text rendering or
   symbol lookup belongs in the ISR. Never substitute the handler's own PC or
   the emulated GBA PC. Record core-1 WFE/idle samples as idle rather than
   treating all core-1 samples as rendering work.
2. Stop and snapshot the histograms after a fixed scene/window; avoid sharing
   writable counters across cores. Map bins to function ranges using the
   **matching ELF** and `arm-none-eabi-nm -n -C` / `addr2line`. Coarse bins can
   cross function boundaries; retain that uncertainty. A randomized interval
   reduces phase-lock bias. Measure instrumentation overhead with profiling
   enabled/disabled on the same scene. Explicitly time higher-priority HDMI
   IRQs because a lower-priority sampling interrupt cannot sample them fairly.
3. **DWT cycle/stall measurements.** RP2350's Cortex-M33 DWT provides cycle,
   load/store, sleep and exception counters. Enable trace/cycle counting on each
   core and measure windows around CPU dispatch, bus access, renderer and wait
   paths. Observe the narrow event counters' overflow limits; a value sampled
   once per second cannot safely represent a high-rate small counter. Use the
   installed RP2350 headers and the datasheet's supported fields rather than
   assuming every optional Arm counter exists. These counters are not mutually
   exclusive utilization percentages.
4. **XIP/bus measurements.** Read XIP access/hit counters over defined windows
   and bus-fabric upstream/downstream stall or contested-access events. XIP
   hardware counters describe the shared flash/PSRAM subsystem, not just the
   emulator's software ROM-page cache. The bus counters are not per-function
   stall attribution. Use controlled runs to distinguish code fetch, external
   data traffic and cross-core/DMA contention; handle saturation/reset.
5. **Controlled A/B tests.** Replay the same input/scene with rendering off,
   HDMI scanout off, UART off, or a small hot working set moved into SRAM, one
   change at a time. A headless run needs a timer/GPIO/UART result path since
   HDMI cannot display while disabled. Compare guest cycles/frames per wall
   second, not monitor refresh. Keep guest timing and input replay fixed.
6. **Instruction/data profiles.** Count hot guest blocks/opcodes and memory
   regions. Combine their execution frequency with on-board cost measurements
   before choosing a decoded-instruction cache, translated block backend,
   PSRAM data cache, selective renderer specialization or scanout redesign.

The [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
documents Cortex-M33 DWT (section 3.7.4.11), XIP performance counters (4.4.4)
and bus performance counters. Profiling should identify the dominant cost;
it does not guarantee that eliminating one cost produces the 8.6x gain needed
to turn 7 FPS into 60 FPS.
