# v33: stable SRAM instruction line

## Board result that motivates this experiment

The user reports that v32 still has no performance improvement. The supplied
profile lists submit_hook 22%, Bus code fetch 11%, CPU Thumb 7%, and CPU
dispatch 8% in an unspecified window; Bad is 0 and unmapped is 36. No new
FPS/Run/Draw/Wait values, total sample N or scene/duration were supplied.
Unmapped is interpreted as a count, not a percentage. Submit still includes
the compiler-inlined WFE renderer wait. These observations do not justify
summing all entries or treating submit time as queue-processing CPU work.

## Instruction path change

V33 keeps a **separate, stable 256-byte SRAM ROM instruction line** in the
compressed provider. An optional provider callback explicitly promises that
this line survives ordinary data reads and prefetch operations until the next
instruction-line request. Only this provider opts in; generic providers keep
their previous reader behavior. This is an interpretation optimization, not
ROM recompilation, JIT translation or native execution of guest instructions.

The cartridge remembers the current line's pointer/tag. Eligible halfword/word
ROM instruction fetches can load the requested scalar directly from it,
without a provider/cache lookup on each hit. A line change copies an existing
SRAM hot-cache line into the dedicated buffer. Ordinary data-cache eviction
therefore cannot leave the interpreter pointing at overwritten bytes.

Only the existing successful ROM-prefetch paths in `Bus::read_code` use the
new method. RAM, BIOS, ordinary data accesses, DMA, failed prefetch/bounds and
EEPROM keep their existing bus paths. The first 256 ROM bytes, which contain
RTC/GPIO registers, also retain the ordinary cartridge reader. Partial final
lines are padded by the provider but accesses still have complete bounds
checks. A failed callback falls back to the ordinary reader. Replacing a
provider clears its instruction pointer, tag and counters before another
instruction can use them. There is one instruction-window consumer on core 0.

The bus still performs the same guest cycle accounting, prefetch transitions,
open-bus/latch updates and prefetch hints. Fetch wrappers and unchanged
ROM-size/WAITCNT getters are inlined to remove calls across flash/SRAM. CPU
instruction semantics are unchanged. The renderer, frame count, clocks,
original BIOS, UART and HDMI driver remain unchanged. Rendering is not skipped.

This follows the direct-fetch design direction identified in
[MGBA-REFERENCE.md](MGBA-REFERENCE.md), adapted to this compressed ROM cache;
no mGBA code is copied.

## Evidence and memory cost

The final 6000-frame Sonic regression passes CPU/banked/flags, exact cycles,
bus/prefetch/latches, EWRAM/IWRAM and every RGB565 pixel/protected margin against
the synchronous ordinary-ROM reference. Display target replacement at frame
3000 passes. All 20 checkpoints still match the original-BIOS v30 baseline.
Final PC is `080bc248`, hash `5ba30470`, cycles 1685293130 and input reads
11737; worker scanlines remain 953015.

The new window records 83204183 hits and 1910528 fills: **97.76%** of eligible
window accesses reuse the current line. SRAM hot-cache hits fall from
114092759 to 30888576, exactly 83204183 fewer lookups; hot misses remain
184937 and backing page misses remain 1051. A fill still copies 256 SRAM
bytes, so these counts do not translate directly into an FPS multiplier.
This is a scripted native sequence, not physical-board performance evidence.

Focused checks pass 263168 instruction-window reads across line boundaries,
unaligned requests, partial ends and bounds; stable hits with intervening data
reads, failed-provider/header fallback and ROM replacement; plus 60000 mixed
bus comparisons covering cycles/latches/prefetch, aliases, GPIO, DMA and EEPROM.
The real compressed provider separately verifies that instruction bytes survive
ordinary hot-cache conflict/eviction. Existing CPU/fetch/BIOS/video/register/
DMA/independent timer tests continue to pass. These are regressions, not an
independent physical-GBA or full-game accuracy proof.

The build reports 230356 static SRAM bytes, 352 more than v32, leaving
277548 bytes of heap capacity before dynamic allocations. There is also
dynamic SRAM growth: a 256-byte buffer in CompressedRom and the cartridge's
callback/pointer/tag/counters. Final ELF debug types report CompressedRom
5124 bytes, Cartridge 164 bytes and Emulator 83456 bytes. The emulator grows
24 bytes over its preceding 83432-byte allocation. PSRAM arena and stack
reservations remain unchanged. The final sampler map contains 824 functions.
Firmware code inspection confirms that fetch wrappers and the two getters
have no remaining standalone symbols/call veneers.

## Board test and fallback

User's v33 result: PC0 submit_hook 22%, Bus code fetch 12%, CPU dispatch
8%, Wait 24 ms/frame. Scene/duration, total sample N, alignment of the
third-hotspot window, FPS/Run/Draw and IFetch hit/fills were not supplied.
This confirms startup and profiling on the board but does not establish an
FPS improvement. Submit includes its inlined renderer wait; Wait is part
of Run. The v32 code-fetch 11% and v33 12% readings are not controlled
absolute-time comparisons, and cannot prove a regression.

Flash `output/GBAFeather-Sonic-v33.uf2` using BOOT + RESET. Banner:
`BOOT v33 - SONIC / INSTRUCTION LINE`. Keep the same slow Sonic screen for
at least 20 seconds. Record FPS, Run, Draw and Wait, plus `IFetch hit` and
`fills/s`, and the top PC0 functions. The top diagnostic row now rotates in
four five-second phases: HDMI IRQ stats, sampler stats, third hotspot, and
instruction-window stats. The first two PC0 hotspots remain at the bottom.

IFetch is the recent hit percentage among eligible ROM instruction-window
accesses, not all guest memory reads or all instruction fetches. Fills/s is
the recent refill-attempt rate. A high hit ratio proves the path is being
used, not that frame time necessarily improves; compare the same scene.

The unchanged working v32 UF2 is the primary manual-reflash fallback:
`output/GBAFeather-Sonic-v32.uf2`, SHA256
`c65934029cea3ad2e224afd362a44d350439b109bb01bac3b02af926725c6719`.
V31 and v30 also remain available. No v33 board FPS improvement or 30/60 FPS
result is claimed. There is no automatic dual-image recovery. Modified core,
ROM, original BIOS and firmware remain private; no public push is part of
this experiment. Full validation is in `output/performance-v33-summary.json`.
