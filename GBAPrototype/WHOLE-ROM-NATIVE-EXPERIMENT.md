# Whole-ROM native recompilation experiment

Latest hardware update, 25 October 2026: the Native HDMI Test 3 SRAM timing-data
control still develops about 84,066 scanout IRQ/s during ROM metadata scanning,
before guest execution starts. It was stopped before a measured FPS window;
V52 restoration and runtime fingerprint were verified. See the
[reference-guided plan](REFERENCE-GUIDED-OPTIMIZATION.md) for the verified sources,
test evidence and separate CPU/memory experiment queue. Earlier dated sections
below retain the history and their original measurement scope.

20 October 2026. A separate agent built and ran a broader Sonic recompilation
experiment alongside the RP2350 optimization tests. It uses
[GBA-Recomp](https://github.com/JRickey/gba-recomp/tree/de4edf59b872d887046d6a3b005e2df551b6d44c)
at the pinned commit shown in that link. The upstream engine/runtime was
unmodified for this baseline; a private headless harness drives the real BIOS,
input policy 3, 5,000 warmup frames and measured frames 5,001–5,180.

## What ran

The experiment analyzes the ROM's recursively discovered code graph, seeded by
observed transfers, BIOS vectors and RAM snapshots. It compiles every discovered
block without the existing emulator's hot-catalog limit: 27,926 blocks, including
26,258 ROM, 1,532 BIOS and 136 IWRAM blocks. This is a whole-ROM analysis attempt,
not proof that every path in the full game has been found. The shipped database
has no complete function map for this Sonic image.

The resulting three C files total 38,610,260 bytes; the desktop native DLL is
8,550,912 bytes. Sonic runs to frame 5,180 and the final image was inspected.
Generated source, translations, snapshots, BIOS/ROM bytes and game images remain
private and excluded from the repository.

## Actual desktop comparison

Three alternating pairs use the same upstream runtime and requested workload.
Timing excludes frame hashing and the hidden-interpreter-call counter; separate
instrumented runs establish correctness and fallback counts.

| Execution mode | Median seconds for 180 frames | Relative speed |
| --- | ---: | ---: |
| Upstream interpreter | 0.408648 | 1.00× |
| Compiled native/interpreter hybrid | 0.229136 | 1.7834× |

With per-frame hashes and shell counters enabled, corresponding medians are
0.450413 and 0.280722 seconds: 1.6045×. These are desktop measurements against
that tool's own interpreter. They are not Feather FPS, a comparison against
gbaembedded, or a multiplier to apply to the board's current FPS.

All 240 sampled framebuffer hashes match the upstream interpreter, including
every measured gameplay frame. CPU/state hashes match only 5 of those samples;
frame-boundary PCs differ, and final IWRAM hashes differ. Translated blocks use
a different timing boundary. No exact-state crosscheck against the project's
current core was performed, so this is not an accepted replacement backend.

The run is still **hybrid**. During the 180 measured frames it uses 276 explicit
fallback instructions and 125,962 interpreter calls inside generated native
functions. Across the full boot/replay there are 16,422 explicit fallbacks and
1,027,442 hidden calls. A strict no-dispatch-fallback test fails during BIOS boot
at frame zero. Finding all dispatch entries alone would not remove the hidden
instruction shells. A separate follow-up measures their opcode classes and tests
removal; the baseline above stays frozen.

## Zero-interpreter follow-up, 21 October

A separate fork retains the hybrid baseline above unchanged. It emits the
observed register shifts, PC writes, loads, PSR/banked-register operations,
software interrupts and BIOS operations directly, using semantic helpers where
needed. Helpers do not fetch/decode a guest opcode or invoke the generic executor.
ISA-aware entry discovery also fixes the measured dispatch misses.

The final follow-up runs the full **5,180-frame workload with zero dispatch
fallback instructions, zero hidden interpreter shells and zero guest instructions
executed through the hardware-service path**. A second run configures any such
interpreter use to abort and also completes. Every frame's image, CPU/RAM state
hash and PC matches the frozen native baseline, including all 180 measured frames.
The primary agent independently checks both complete CSVs and the strict run log.
The state hash covers r0–r15, CPSR, EWRAM and IWRAM; it does not cover every banked
register or every IO/video/audio internal field.

A subsequent comparison pauses the other agents' compiles and pins all nine
child processes to the same CPU, with normal priority. Each run cold-boots the
real BIOS and measures the same 180-frame interval; frame hashing and shell
counters are disabled during timing. Three rounds produce:

| Mode, same comparison batch | Median seconds for 180 frames | Speed versus own interpreter |
| --- | ---: | ---: |
| Upstream interpreter | 0.401910 | 1.00× |
| Frozen hybrid native baseline | 0.246718 | 1.63× |
| Zero-interpreter follow-up | 0.236553 | 1.70× |

Scatter remains: follow-up windows range from 0.234931 to 0.269873 seconds,
while interpreter windows range from 0.401591 to 0.469561. Its median is 4.30%
faster than the hybrid in this batch, but the trials do not establish statistical
significance. The substantive new result is zero-interpreter execution on the
tested path. The earlier 1.7834× hybrid batch remains separately recorded; these
desktop ratios must not be multiplied by current Feather FPS. The quiet batch
is retained in `build/whole-rom-native/quiet-comparison/results.json`.

Boot code copies temporary instructions into BIOS scratch RAM. The follow-up
handles 50 observed instruction keys with 61 content versions, preserving code
guards and rejecting unknown versions. Tests accept all 61 known versions and
reject all 50 mutated cases. Another 6,600 instruction cases over 55 sites pass
with the generated-code interpreter callback configured to abort.

This is **zero-interpreter execution for this tested path**, not a complete
conversion of every path in Sonic. All 5,180 frame images also match the upstream
interpreter, but only 7 CPU/state/PC boundaries do: the pre-existing native timing
difference remains. This follow-up preserves that native baseline; it does not
repair or validate its timing against the current project core. ARM output,
footprint and performance must be measured separately for the changed translation.

The follow-up also compiles to Cortex-M33 Thumb-2: 4,530,436 text bytes and
223,856 read-only bytes, producing a 4,754,296-byte aligned image. Its verified
zlib stream occupies 1,577,536 bytes. Native code plus the compressed ROM then
uses 6,330,603 Flash bytes, leaving 2,058,005 before runtime/loader. Expanded code
plus 555,008 bytes of fixed guest buffers leaves 3,079,304 bytes of PSRAM. These
are capacity figures, not measured execution. The runtime API now has 15 function
pointers, including five explicit CPU/BIOS semantic helpers; the loader must
handle both the main table and the guarded RAM overlay table at their linked
addresses.

Private evidence is retained in
`build/whole-rom-native/fallback-elimination/stages/` and
`build/whole-rom-root-verification.json`. A runtime adapter and a standalone
compressed-image loader are separate work; neither establishes board gameplay.

## Actual Cortex-M33 output and storage model

The generated C also compiles to genuine Cortex-M33 Thumb-2 with the installed
ARM GCC. Both `-Os` and `-Oz` produce identical objects: 4,455,300 bytes of text
and 223,416 bytes of read-only tables, **4,678,716 bytes total**. With the existing
4,753,067-byte compressed ROM, a raw Flash layout needs 9,431,783 bytes, exceeding
8 MiB by 1,043,175 bytes before firmware/runtime. No complete firmware was built.

A tested compression/link model provides another option. A fixed-address native
image occupies 4,679,352 bytes after alignment and compresses to 1,560,203 bytes
with zlib level 9; byte-for-byte decompression is verified. Compressed native
code plus compressed ROM then occupies 6,313,270 Flash bytes, leaving 2,075,338
bytes for firmware, loader and metadata. Expanded code plus the stock fixed
guest-memory/framebuffer allocation leaves 3,154,248 bytes of 8 MiB PSRAM.

This is a **capacity model**, not a working board implementation. It requires
less ROM backing cache, a reserved executable PSRAM image, correct fixed Thumb
function addresses, cache coherency, a loader and a suitable runtime. PSRAM
instruction-fetch behavior and performance are not yet measured. The model's
base address must not be used as an arbitrary write destination: allocation or
linker reservation must first establish ownership of that region.

The stock Rust runtime does not build unchanged for bare metal: the attempted
target build fails because dependencies expect `std`. Its 16 MiB ROM Vec alone
exceeds this board's PSRAM. Fixed guest buffers also need relocation. A port or
an adapter to the existing bus/PPU/ROM pager is necessary; emitting Thumb-2 is
only one part of that work.

## Physical Feather milestones, 21 October

The one-shot execution probe passes on the actual Feather at 240 MHz CPU and
30 MHz PSRAM. The same 16-byte Thumb-2 arithmetic function passes directed and
checksum tests in Flash, SRAM and allocated PSRAM. Owned allocation bounds,
alignment, call counts and checksums are independently checked after capture.
MPU_CTRL is zero; no permissions are changed, and controller state is restored.

| Placement | Hot cycles/call, 100,000 calls | Cold cycles/call, 256 calls |
| --- | ---: | ---: |
| Flash | 14.029 | 197 |
| Internal SRAM | 14.035 | 37 |
| External PSRAM | 14.034 | 409 |

These include common loop/indirect-call/timing overhead. The tiny hot function
fits in cache; the result does not predict the full image's throughput. The cold
PSRAM call is about 1.70 microseconds in this test, including that overhead.
The raw USB capture and independent checks are in
`logs/perf-lab/psram-execution-probe-01/`.

A second physical firmware decompresses the complete 4,754,296-byte stage3 image
into its linker-owned PSRAM reservation. It passes exact expanded-image SHA256,
27,930 main-table rows and 50 RAM-overlay rows, including another full SHA/table
verification after cleaning and invalidating the owned cache range. Load and
verification take 9,434 ms. All required terminal fields pass, controller state
is restored, and **zero native game entries execute** in this loader test.
Evidence: `logs/perf-lab/native-image-loader-stage3-01/`. Both physical workers
restore V52 and verify the running application before releasing the board.

## Bare-metal runtime port in progress

A new private fork compiles the runtime with no_std/alloc for Cortex-M33. Its
hosted and no_std-core controls each preserve all 5,180 frame image/state/PC
hashes and zero-interpreter counts. External ROM backing also passes that
stage3 replay with an 8 KiB file pager and an empty resident ROM Vec. These are
desktop correctness controls, not RP2350 runtime execution. A bounded C API is
being checked so USB and input work can resume between execution slices.

The measured ARM32 Machine object is 944 bytes; its guest buffers are heap
allocations. Static constructor call frames total about 3 KiB before allocator,
libc and interrupt overhead, rather than constructing a 555 KiB stack object.
An additional 1 MiB decode-cache allocation was found. The explicit small-cache
feature reduces it from 32,768 entries to 64, saving 1,046,528 bytes while keeping
the same pure decoder on collisions. A 500,000-case cache test, the default core
tests and the full strict replay pass. Guest work is not removed.

The bounded runtime also passes all 5,180 frames when resumed with budgets of
1, 127, 1,024 and 4,096 outer iterations. It then passes the exact frozen
compressed C++ ROM provider, with 2 MiB backing and 32 KiB hot storage, rather
than only the file-pager substitute. No interpreted guest instructions occur.

The final Rust archive uses the compatible soft C ABI and links successfully
with Arduino's actual Feather toolchain and the board allocator; no ABI-mismatch
suppression is used. That ROM-free link occupies about 146 KiB Flash and 30 KiB
static SRAM, including the 16 KiB allocator pool. These figures exclude native
code, ROM, BIOS and final display integration. Constructor sizing also changes
slightly in that final feature configuration; its known call chain is about
3.9 KiB before allocator/interrupt overhead. Hardware stack high-water remains
unmeasured. The first full board image uploaded and verified at 00:58:22 Dublin
on 21 October, but the Feather did not re-enumerate over USB and no startup
record was captured. Root stopped the capture and attempted V52 recovery once;
the board was unavailable in both runtime and serial-pinned BOOTSEL modes, so
further hardware work requires manual BOOT/RESET recovery. The headless image
intentionally contained no HDMI driver. This is a startup failure with an
unresolved cause, not a measured failure of native guest execution. Its capture
is `logs/perf-lab/native-runtime-board-01/`. A separate build with HSTX startup
diagnostics and frame-boundary game display is being prepared; there is no
native board FPS result yet.

The offline startup audit then found a concrete code-placement regression.
Arduino-Pico's `psram_init` enables QMI direct mode and computes a signed
64-bit division before disabling direct mode. In the failed image that call
resolves through an SRAM veneer to the Rust archive's `__aeabi_ldivmod` and
division helpers in Flash. The proven loader instead resolves the SDK/libgcc
division closure in SRAM, as required during this XIP-sensitive interval.
This is a definite residency hazard and a likely explanation for the missing
USB startup, but the causal diagnosis still needs a successful board retest.
The new HDMI clone is being linked to restore SDK/libgcc ownership before the
Rust archive; final disassembly must verify the complete helper closure rather
than merely moving its entry wrapper. The failed image is preserved unchanged.

## Independent official mGBA reference, 21 October

The official [mGBA source](https://github.com/mgba-emu/mgba/tree/fd9dc5c2cbf234365068855851107e762fc6211b)
is pinned, built headlessly and executed through all 5,180 frames with the same
real BIOS, ROM and policy-3 input pulses. Its upstream source is unchanged,
software rendering is enabled without frameskip, and no save or persisted
frontend settings are imported. Captures use canonical little-endian RGB555
240×160 pixels and the same FNV64 picture hash. No frame shift or input timing
change was made to obtain agreement.

The stock/native runtime matches 1,416 same-index picture hashes, and a separate
existing F direct-framebuffer fixture matches 632. These are not accuracy
percentages: static/blank frames can match while one shade or animation phase
can invalidate an otherwise similar image hash. Existing F matches pictures
1–130; its first difference at 131 is 28 pixels with a one-unit red-channel
discrepancy in the BIOS logo. Sampled non-PC registers already differ at frame
3 at the same 758,912-cycle timestamp. Raw PC fields also have different
pipeline conventions, so a PC offset alone is not counted as a CPU failure.

At frame 5,180 all three captures show Sonic demo play, with different camera
and game progression. mGBA's observed time is 1,454,957,525 cycles versus
1,454,957,504 for F; a large frame-origin offset alone does not explain the
divergence. The reference does not localize the cause or serve as physical
hardware proof, and the F fixture does not exercise board ScanlineWorker/HDMI.
It establishes that own-interpreter/native picture parity is insufficient to
claim independent compatibility or equivalent 60 FPS gameplay.

Private evidence is frozen under `build/reference-mgba/`, including all-frame
hashes/raw pictures, selected PPMs, raw register/cycle observations, comparison
contact sheets, source/input identities and retained setup failures. The initial
debug-disabled global-cycle observer was corrected to actual master time, and
the idle option was made explicit; all picture hashes remained identical.

Separately, the legacy F-based typed ARM extension now completes its board
trial at **7.622097 FPS**, **+1.74734% versus F's 7.49120** in one sample, with
strict legacy endpoints passing. Run/Draw/Wait are 124.57575/24.61482/9.532294
ms per frame; Run outside Wait is 115.0434556 ms. This is a small observed
legacy-path change, not a whole-ROM runtime measurement or statistical proof.

## Next concrete tests

The first compiled runtime adapter exposes why this is a port rather than a
relink. Generated C executes against the current Bus/PPU with correct register
and memory values in the positive fixtures. However, a mixed block consumes
9 cycles versus the reference's 18, and even a two-cycle ALU fixture leaves
different instruction-fetch state. An interrupt due between two instructions
is handled too late by the unchanged block-boundary tick. A real Sonic Thumb
epilogue also returns with the correct registers, banked state and SP but takes
6 cycles versus 13 and leaves different bus state. These are retained negative
tests, not accepted timing changes. The direct adapter is not a working
replacement for the current CPU backend.

- Diagnose the first native image's USB-less startup, build visible HSTX startup
  diagnostics and game display, and test only after manual board recovery.
  Report timing only after health, frame-window and strict execution checks pass.
- Keep the completed PSRAM execution and owned-loader checks as capability
  evidence; measure full-image execution separately from the tiny hot function.
- Extend strict zero-interpreter coverage beyond this fixed boot/demo path.
- Retain the compiled adapter's timing failures as acceptance tests; evaluate a
  bare-metal port of the native runtime rather than silently accepting them.
- Localize the early BIOS CPU-state and first visible mGBA differences; keep
  independent compatibility separate from own-runtime image parity.
- Validate display/gameplay integration separately after the headless test;
  record board FPS separately from the desktop ratio.

Private reproduction commands, all raw runs/errors, compiler flags and artifact
hashes are retained in `build/whole-rom-native/README.md`, `results.json`,
`compression-results.json` and `hashmanifest.json`. They contain or identify
private game-derived artifacts and are not copied into the public repository.
No 60 FPS result or zero-interpreter coverage of the entire game is claimed.
