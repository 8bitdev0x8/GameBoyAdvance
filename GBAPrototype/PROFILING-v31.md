# v31: on-board core-0 PC sampling

**Subsequent board report:** Bus read 17%, CPU ARM 12%, Cartridge read_bios
12%, N2561, Bad0, unmapped0, displayed cost 0 ms/s; other reported windows show
submit_hook 29%, Bus read 12% and Render wait 18%. Submit includes an inlined
WFE wait in the final assembly. Windows are not additive; zero cost is rounded.
See [PERFORMANCE-v32.md](PERFORMANCE-v32.md) for the resulting targeted changes.

## Board findings before this build

The supplied original BIOS boots in v30. The user reports 5-7 FPS, PAD ON,
VBskip 12%, Run 113, Draw 30, Copy 0 and Wait 32 ms/frame. HDMI reports
31507 interrupts/s and 146 ms/s in its handler. Dependent-read loop costs:
SRAM 32 KiB 21 ns, PSRAM 32 KiB 767 ns, PSRAM 256 KiB 1188 ns. The last
value was clarified from an initial transcription of 118.

Scene/duration were not supplied. These readings resemble the prior v28
results, but do not establish a controlled same-scene BIOS speed comparison.
Wait is inside Run; Draw overlaps core 0. Run minus Wait is about 81 ms,
including interpreter, bus, event and interrupt work, not pure guest execution.
The memory loop measurements are not frame-time attribution. Removing only
the measured 14.6% IRQ-handler cost would ideally yield about 1.17x, with all
other costs fixed; it cannot alone turn 5-7 FPS into 60 FPS.

## What changes in v31

This is a diagnostic build using the same original BIOS and emulator as v30.
It adds a core-0 hardware timer interrupt at roughly 485 samples/s, with a
dithered 1933-2188 us interval plus handler duration. Sampling starts only
after the first emulated frame. The IRQ captures the interrupted native M33
PC before a C prologue changes MSP; it does not sample the guest GBA PC.
There are no interpreter, renderer, clock or ROM-cache optimizations in v31.

A fixed 32 KiB flash table maps PCs to function ranges. The build first links
an empty table of the same size, generates ranges from that exact ELF, links
again, then checks every range against the final ELF. A mismatch stops the
build. Histograms use 1024 saturating 16-bit counters in SRAM; each five-second
window is independent. Lookup uses a bounded binary search. There is no
allocation, text output, floating-point calculation or emulator call inside
the sampler. The handler and its entry trampoline run in SRAM.

The sampler claims a free SDK hardware alarm instead of assuming an alarm
number. It has lower priority than HDMI and never preempts the scanout ISR.
Only its own IRQ is masked while collecting/resetting counters; HDMI continues.
Unsupported security/frame layouts and invalid/out-of-SRAM frames are counted
and rejected before reading them. Standard secure integer and FP frames both
have core PC at offset 24. No FP-register save/restore is attempted by the sampler.

## Reading the HDMI results

Flash `output/GBAFeather-Sonic-v31-Profile.uf2` with BOOT + RESET. Banner:
`BOOT v31 - SONIC / PC SAMPLER`. The existing startup memory benchmark still
appears. Keep the same slow Sonic scene visible for at least 15 seconds.

The two bottom rows show `PC0 #1` and `PC0 #2`, their function labels and
percentages. The row above the game rotates every five seconds between HDMI
IRQ statistics, sampler diagnostics and `PC0 #3`. Type all three function
labels/percentages and the diagnostic line:

* `N`: samples in the last completed window.
* `Bad`: rejected exception frames.
* `?`: valid PCs outside mapped function ranges.
* `Cost`: measured handler-body elapsed ms/s, rounded down; includes nested
  higher-priority interrupts and excludes exception entry/exit/wrapper work.

If no alarm is available, the HUD reports the sampler as unavailable and the
game continues. `PC0 collecting` lasts until a complete sample window exists.
If Bad or unknown counts are substantial, investigate the sampler/map before
using its ranking to justify an optimization.

## Interpretation and limits

Percentages are shares of delivered core-0 samples, including rejected/unknown
samples in the denominator. They are exclusive PC locations, not inclusive
call-tree time. Low-priority sampling cannot fairly measure the highest-priority
HDMI ISR; use its existing independent elapsed counter. Interrupt masking,
memory stalls and late delivery can bias samples. Dithering reduces periodic
aliasing but does not remove it. Core 1 is not sampled; Draw/Wait remain useful
there. This is not a complete CPU-utilization or memory-bandwidth profiler.

The next action depends on the hottest functions: instruction-window/dispatch
work for CPU/fetch/cache hotspots, deadline/service work for scheduler hotspots,
and renderer synchronization work for Render wait. See [MGBA-REFERENCE.md](MGBA-REFERENCE.md)
for concrete mGBA design comparisons. No FPS gain is promised for this
measurement build, and its extra work may slightly reduce FPS.

The already-working original-BIOS v30 UF2 is the primary fallback. Reflash
`output/GBAFeather-Sonic-v30-OriginalBIOS.uf2` manually if v31 fails; v28 remains
another confirmed-working fallback. V29's hard fault is still unexplained and
its extra SRAM placements are excluded. New firmware, private core and BIOS
remain local; no public push is part of this experiment.

## Rebuild and checks

```powershell
./GBAPrototype/build.ps1 -Profile -BiosPath '<user-home>/Downloads/gba_bios_202501/gba_bios/gba_bios.bin'
```

The default build omits sampling unless `-Profile` is supplied. Generated
profile tables and BIOS headers are ignored. Focused host tests cover secure
thread/IRQ frames, MSP/PSP, integer/FP core-PC placement, bad/security/alignment/
bounds rejection, and binary lookup against a linear reference across gaps and
boundaries. The core is byte-identical to v30, whose 6000-frame original-BIOS
regression remains the core's validation evidence. These host tests cannot
exercise actual RP2350 interrupt delivery or establish board FPS.

Technical references: Arm's
[Cortex-M33 exception model](https://documentation-service.arm.com/static/5f16e93d20b7cf4bc524af1d)
and the installed Pico SDK hardware timer/IRQ implementations. Exact memory,
UF2, final symbol-table, machine-code and fallback checks are recorded in
`output/profile-v31-summary.json`.

The final image maps 832 function ranges. It uses 229620 static SRAM bytes,
2512 more than v30, leaving 278276 bytes of heap capacity before dynamic
allocations. The sampler's C handler uses 16 compiler-reported stack bytes,
in addition to hardware exception stacking. Inspection confirms the naked
SP-capture trampoline and handler are in SRAM, with no outlined calls or FP
instructions in the handler. UF2/vector/stack/ROM/BIOS and initialized-SRAM
checks pass. These do not establish physical-board interrupt behavior.

Firmware SHA256:
`20bc9eb2fb8e8b8584d93e57ae4fc1e2e6a25c7944b06098b975fbb96fb532ea`.
