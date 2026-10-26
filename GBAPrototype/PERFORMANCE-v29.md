# CPU/bus bottlenecks and v29

**Board result:** the user reported `CPU HARDFAULT`, fault status `00000000`.
The last numbered stage and root cause are unknown. V29 is a failed board
experiment despite its successful native regressions. The following records
the attempted changes; v30 restores the confirmed v28 core for a separate BIOS
test. Use v28 for recovery. Its original UF2 and v29's private source/archive
remain preserved.

The last user-reported v28 result was about 7 FPS, PAD ON, VBskip 12%,
Run 113, Draw 22, Copy 0 and Wait 14 ms/frame. Wait is included in Run;
rendering overlaps core-0 work. The approximately 99 ms outside renderer
waiting is not pure instruction execution: it also includes nested interrupts,
guest event handling and bus operations. V29 hardware FPS is unavailable because
the user reported a hard fault.

## New physical evidence: the user's v28 video

The private video `GBA.mp4` (SHA256
`c42dc111581328c467b5808c22b4404518139076611916445d70ba454670b3e9`)
shows the benchmark and later runtime HUD. Extracted frames remain private;
no game imagery or video is included in the public repository.

| 32-bit benchmark, ns/operation | Sequential read | Dependent read | Sequential write | Scattered write |
| --- | ---: | ---: | ---: | ---: |
| SRAM 32 KiB | 45 | 21 | 38 | 49 |
| PSRAM 32 KiB | 387 | 769 | 967 | 1403 |
| PSRAM 256 KiB | 408 | 1187 | 1114 | 2156 |

These are CPU-observed loop costs with HDMI running, not isolated raw-chip
latency. The dependent-read penalty is approximately 36.6x for equal 32 KiB
windows and 56.5x for the larger PSRAM working set relative to SRAM. That is a
real access penalty, not a frame-time multiplier. Different loops have different
overhead, so SeqR and DepR should not be compared as raw memory latency.

The runtime HUD shows approximately **147 ms/s inside the original HDMI
handler**, IRQ frequency near 31500/s and a cumulative reset count of 5.
This is approximately 14.7% of one second in that handler, excluding some
entry/exit/wrapper cost. It is already included wherever it interrupts timed
CPU work. If precisely this cost vanished while everything else stayed equal,
the idealized overall gain would be about 1.17x, not the 8.6x needed for 7→60 FPS.
DMA/memory-contention changes could have other effects; none has been measured.

The video includes different intervals: a BIOS credit screen at 37.3 FPS
(Run23/Draw24/Wait19), a name-entry screen at 10.4 FPS (Run90/Draw38/Wait37),
and a SEGA intro at 7.6 FPS (Run123/Draw34/Wait32). Copy is zero throughout
these samples. These snapshots are not a sustained gameplay average and do not
supersede the separate typed 7 FPS / 113 / 22 / 0 / 14 report. Decode is zero
in some intervals and 10 ms/s in the later intro sample. The retained reset
count does not establish when each reset happened or its cause.

## Confirmed avoidable work and changes

1. **Hot helpers still crossed into flash.** V28 assembly placed every CPU
   fetch wrapper in flash, as well as the tiny WAITCNT getter, shift dispatcher
   and ordinary GamePak reader. V29 places the wrappers/shift/GamePak reader in
   SRAM and inlines the unchanged WAITCNT getter. CPU source behavior is
   unchanged: its delta is three placement attributes. V29 assembly shows
   direct SRAM calls and no remaining WAITCNT symbol/veneer.
2. **ROM words were read twice.** Ordinary non-DMA in-bounds ROM word data
   reads fetched the same aligned word again to fill the ROM latch. V29 reuses
   the existing word. Byte/half, bounds, DMA and code-fetch paths retain their
   rules. GPIO reads are pure projections; RTC clock changes occur on writes.
   This assumes ROM-provider bytes remain stable, as they do for CompressedRom.
3. **Unchanged registers still joined the renderer.** V28 avoided dirtying
   on equal assignments, but Bus::write_io waited before reaching that check.
   V29 reads pure storage state before the join. Exact unchanged writes skip
   both mutation and join. Changed, status, unknown and partly unknown word
   writes retain the original path. Byte merges still use the original masked
   readback, then compare against raw storage; affine halves retain sign/merge
   rules. The preflight and register reader run in SRAM without outlined calls.

No driver, clock, linker, render queue, guest timing, ROM data or frame count is
changed. There is no JIT or decoded-instruction cache. More pending jobs would
require state snapshots/coherence; queueing unsynchronized reads is not safe.

## What the scripted native run measures

The 6000-frame original-ROM/ordinary-fetch/conservative-video/reference-latch
path matches compressed-ROM/fast-fetch/threaded/direct-video execution in
full CPU/banked/flags, exact cycles, bus/prefetch/latches, EWRAM/IWRAM and every
RGB565 pixel/margin, including display replacement at frame 3000. All 20
checkpoints match v28. Final PC `080bcca6`, hash `5ba30470`, cycles 1685292894,
KEYINPUT reads 11861 and worker jobs 958723 are unchanged.

- 1,033,387 storage-register half assignments bypass the renderer join.
- ROM hot-cache hit lookups fall from 117,386,291 to 113,235,839: **4,150,452
  duplicate lookups removed**. Cache misses stay 187416 and backing misses 1048.
- The earlier video-write and decoded-tile work counts remain unchanged. This
  optimization reduces joins and ROM work; it does not skip game frames.

New native-only counters measure CPU **Bus::read calls**, excluding instruction
fetches. These are frequency counts, not access durations, byte bandwidth or
all hardware traffic:

| CPU data-read region | Calls | Share |
| --- | ---: | ---: |
| IWRAM, SRAM | 11014527 | 49.37% |
| MMIO | 5504542 | 24.67% |
| ROM | 5358263 | 24.02% |
| EWRAM, PSRAM | 410233 | 1.84% |
| BIOS | 22150 | 0.10% |
| OAM / save | 240 / 130 | less than 0.01% each |

The sequence reaches language/name entry, not a complete level or game. Renderer
and HDMI memory traffic is outside these CPU counters; some optimized DMA paths
also bypass Bus::read. The result supports investigating interpreter/IWRAM/MMIO
overhead but does not establish PSRAM's frame-time share in other scenes. The
physical benchmark confirms PSRAM is slower; frequency and duration must both
be considered before adding a data cache or changing its clock.

Focused checks pass 24030 display-register cases, 1292 ROM/provider/GPIO/EEPROM
cases (including 72 one-provider-read word cases), the existing 45003 video
write/read and 600 DMA cases, 8160 Thumb cases, 30000 fetch cases and 60000
timer comparisons. Another 240 PPU fixtures match the preserved independent
v21 renderer. These are same-core regression checks, not physical-GBA accuracy
or hardware FPS proof.

## New board timing to read

V29 alternates the ROM status row with **Exec / Event / Wait ms/f** every six
seconds. Exec times cpu_run_until inside run_until, including data access,
CPU-local timer/DMA handling and interrupts. Event times the following scheduler
update and service_due_hardware, including render submission/waits, hardware
events and schedule refresh. These are disjoint phase windows within core 0's
run_until calls, not per-function instruction profiles or utilization buckets.
Wait can occur inside either and the outer frame drain; IRQ time is nested.
Do not add Wait or IRQ time again. Draw overlaps these phases on core 1.

The instrumentation adds three timer reads per run_until iteration. Counters
use unsigned elapsed arithmetic; reset is not a cumulative lifetime profiler
reset, and the added phase counters cover run_until, not the separate run_frame
or step_scheduler_event APIs. Existing Run/Draw/Copy/Wait, memory benchmark,
IRQ timing and input behavior remain available. PC sampling/DWT/bus counters
remain further work rather than enabled features.

## Next larger candidate: DMA scanout

Adafruit's [CircuitPython RP2350 video backend](https://github.com/adafruit/circuitpython/blob/main/ports/raspberrypi/common-hal/picodvi/Framebuffer_RP2350.c)
demonstrates narrow DMA pixel duplication and a precomputed command list, with
the final command triggering an interrupt. This provides a concrete reference
for replacing per-line CPU work. It is not enabled in v29. A port must budget
the command list in already constrained SRAM, preserve timings/pin mapping,
handle buffer replacement/recovery and be tested on the actual monitor. The
current installed HSTX driver remains unchanged.

A faster CPU backend is another larger change. The new timing buckets should
separate execution from event overhead before that redesign. Reducing silent
APU scheduling frequency is not yet accepted: it can change the interpreter's
current instruction-batched hardware-service phase and needs a timing audit.

V28's confirmed UF2 remains the fallback. The v29 image and modified core
archive are local-only and must not be pushed to public Git.
