# V53: measured work reduction, reproducible scenes, and pending board results

This development round separates correctness, local performance experiments, and physical RP2350 measurements. Healthy matched demo captures progress from **6.80 FPS baseline to 7.04 with memory/renderer changes, 7.26 with the IWRAM fetch leaf, and 7.49 with resolved-color/fused rendering**: **+10.16% overall** with identical guest endpoints. The subsequent typed ARM extension measures **7.62210 FPS, +1.74734% versus F's 7.49120**, with all strict legacy endpoints passing. Each variant has one completed capture, not repeated-trial proof. The first Flash AOT128 image regresses to **5.47 FPS** and is not promoted. The original combined build failed interpreter allocation; the common allocation repair lets these builds complete. The earlier **14.97 FPS** language-menu result cannot be compared as a gameplay gain.

The raw evidence is retained locally under `build/` and `GBAPrototype/logs/perf-lab/`. `../build/v53-experiment-index.json` inventories evidence paths, SHA256 hashes, byte sizes, recorded outcomes, and the time of the inventory. It references existing files without copying the emulator core, ROM, or BIOS. Private build artifacts and game screenshots are evidence for local development; this document does not grant redistribution rights to them.

## CPU follow-up, 21 October

The F-based native IWRAM arithmetic candidate completes the fixed Sonic demo at
**7.54547 FPS**, versus F's 7.49120: **+0.72% in one trial**, with timing noise
unmeasured. Run is 125.843 ms, Draw 24.439 ms and Wait 9.398 ms per frame;
Run outside Wait is 116.445 ms. Exact CPU/RAM/video endpoint hashes and guest
cycles match. Runtime counters prove 113,506 native calls and 759,307 guest
instructions, including 52,867 fast calls and 475,803 fast instructions, with
zero native exits or changed-code rejections. This does not support a large
whole-game speedup. Evidence: `v53-arm-native-demo-sequence-02/`.

Early catalog-negative admission completes a healthy second attempt at
**6.29336 FPS**: 6.82% above the dedicated SRAM native loop's 5.89174, but
13.37% below interpreter C. Run outside Wait is 136.563 ms/frame. Catalog
lookups fall to 777,344 while native batches, instructions, memory operations,
exits and mismatch counts remain exactly unchanged. The first attempt's HDMI
restart invalidates its FPS claim; that failure remains retained. The second
attempt restores and verifies normal V52 video before releasing the board lock.
Evidence: `v53-early-admission-demo-sequence-02/`.

The bounded fetch-pair candidate subsequently completes at **6.25607 FPS**,
13.88% below interpreter C, with identical guest endpoints, cycles and exact
native-execution counts. Run is 150.498 ms, Draw 32.456 ms, Wait 13.210 ms;
Run outside Wait is 137.288 ms/frame. The native gate falls to 4.93% of eligible
PC samples, but the new batch helper takes 1.49%; overall improvement is not
established. Its 0.59% lower FPS than early admission is within unmeasured noise.
It is not promoted. Evidence: `v53-fetch-pair-demo-sequence-02/`.

A separately validated 4,096-entry native catalog queues after it; actual-M33 local replay
executes 5,440,816 of 9,907,980 guest instructions natively (54.91%). Final
firmware allocation and Flash layout pass validation. Coverage alone does not
establish a speedup; board results are recorded only after capture completes.

That board capture subsequently completes at **4.97239 FPS**, **31.55% below
interpreter C**, with matching CPU/RAM/video endpoints and cycles, healthy
telemetry and all expected native counters. It executes 1,127,885 native batches
and 5,440,816 guest instructions, with zero mismatches. Run outside Wait rises to
175.445 ms/frame, Draw remains 32.530 ms, and Wait remains 13.022 ms. Top eligible
PC samples are translated code 14.41%, native gate 11.83%, native loop 11.78%,
Bus read 5.59% and catalog lookup 3.46%. This rejects broader coverage as a
standalone solution in the present callback-heavy hybrid. It is not promoted.
Evidence: `v53-aot4096-demo-sequence-01/`.

The typed ARM extension is a separate F-based candidate, not combined with the
regressing ROM catalog. Its final firmware validates with 249,744 preallocation
heap bytes and 73,900 bytes of SRAM-loaded code. The 820-byte arithmetic body is
exactly identical to the SDK-tested body. The 1,732-byte sample/flags body differs
only in two validated Thumb BL relocations to the frozen-source SRAM ROM helper;
all remaining bytes match. Both executors activate only at the measured boundary.
One retained preparation failed on Git line-ending conversion, and the first
firmware export used named fields for a counter array; both are corrected before
any upload. Local correctness includes 181 checkpoints, 7,680 edge cases and
6,144 wait-state cases. The physical trial subsequently completes at
**7.622097425903368 FPS**, **+1.74734% versus F's 7.49120**. Run is
124.57575 ms/frame, Draw 24.61482, Wait 9.532294, and Run outside Wait
115.0434556. All strict legacy CPU/RAM/video endpoints and guest cycles pass.
This is one sample, not statistical proof of a repeatable gain; the game thread
remains far above a 16.67 ms budget. Raw evidence is retained in
`logs/perf-lab/v53-arm-typed-demo-sequence-01/`.

The [whole-ROM native experiment](WHOLE-ROM-NATIVE-EXPERIMENT.md) is a separate
runtime/toolchain comparison. Its desktop 1.7834× baseline is not an RP2350 FPS
measurement or a multiplier for these board captures. The serial-pinned PSRAM
execution probe and owned-image loader checks have now passed, with verified
V52 restoration. A tiny-code result or successful image load cannot predict
large translated-image throughput.

The first complete native runtime board image uploaded and verified at
00:58:22 Dublin on 21 October, but no runtime USB or startup record followed.
The attempt was stopped; one automatic V52 recovery attempt failed because the
Feather was absent in both runtime and serial-pinned BOOTSEL modes. Manual
BOOT/RESET recovery is required. This image deliberately had no HDMI driver;
absence of video is separate from the observed USB failure. The exact startup
cause remains unresolved. A new HSTX diagnostic/game-display build is being
prepared. Evidence is under `logs/perf-lab/native-runtime-board-01/`; no native
board FPS is available. This runtime is not treated as equivalent to the current
emulator merely because its own interpreter/native images agree.

An independent official mGBA reference, pinned at
`fd9dc5c2cbf234365068855851107e762fc6211b`, executes the same real BIOS/ROM and
input policy for all 5,180 frames. Its same-index picture hashes match the
stock/native runtime in 1,416 frames and the existing F direct-framebuffer
fixture in 632. These counts are not accuracy percentages. F's first visual
difference at frame 131 is only 28 of 38,400 pixels, with sample red-channel
values differing by one; non-PC register state already differs at frame 3 at
the same 758,912-cycle boundary. Final images all show demo play but differ in
camera/game progression. Frame offsets and inputs were not changed to force
agreement. The default-framebuffer F fixture does not test the board's worker
or HDMI output. Frozen sources, captures and comparison scope are private under
`build/reference-mgba/`; independent compatibility remains unresolved.

## What changed in this round

| Experiment | Intended saving | Correctness evidence | Performance evidence | Current decision |
| --- | --- | --- | --- | --- |
| Memory 1: fixed-width RAM stores | Avoid the generic bus dispatcher for ordinary EWRAM/IWRAM stores | 300,000 RAM fixtures; 6,000-frame differential replay | Single-RAM Cortex-M33 synthetic loop: 11,475,063 to 11,230,063 instructions, 2.14% less work; exploratory desktop timings regressed | Do not promote alone |
| Memory 2: RAM multiple transfers | Also specialize ordinary-RAM ARM LDM/STM and Thumb PUSH/POP/LDMIA/STMIA transfers | 15 audit groups; 6,000-frame differential replay | Thumb multiple-transfer loop: 12,373,146 to 10,809,831 instructions, 12.63% less; ARM loop: 9,757,064 to 8,625,223, 11.60% less | Combined repaired board pair +3.51%; individual contribution not isolated |
| Renderer: zero-strength brightness spans | Avoid second-layer selection when brighten/darken strength is zero; preserve translucent OBJ behavior and bit 15 semantics | 480 PPU fixtures against preserved v21 renderer; 1,024 directed ARM mask cases; 6,000-frame replay | Cortex-M33 compositor microfixtures use 47.42–68.04% fewer instructions, depending on layer/OBJ case | Combined repaired board pair +3.51%; individual contribution not isolated |
| CPU polling experiments | Reduce repeated polling and event-boundary overhead | Focused 64,000-slice tests passed; tail1 combination subsequently failed full VBlank parity | Desktop timings retained as exploratory menu evidence | Reject the failed candidate; focused tests alone were insufficient |
| Generic ARM ALU specialization | Shorten common non-PC, non-flag-setting ARM data-processing execution | 307,200 randomized state transitions; shared pretests; 6,000-frame replay and matching demo endpoints | Synthetic mixer-shaped Cortex-M33 loop: 7,193,057 to 6,806,538 instructions, 5.37% less work | Private candidate; board gain pending |
| IWRAM instruction-fetch leaf | Avoid a function/dispatcher path when fetching actual writable IWRAM code | 69,264 accepted reads with exact bus state; 30,736 side-effect-free rejections; shared pretests, 6,000-frame replay and demo parity | Synthetic Cortex-M33 mixer loop: 12,667,737 to 9,846,679 instructions, 22.27% less work | Board C 7.26469 FPS, +3.21% versus B; one capture |
| APU PCM storage relocation | Recover internal SRAM without changing guest audio events | Four host variants match PCM and guest state; actual no-exceptions constructor compile; worst-case arena audit | Actual Feather type layout saves 8,184 bytes | Common repair in both matched board builds; not separately timed |
| HSTX autonomous DMA ring | Replace approximately 31,500 scanline IRQs/s and CPU pixel duplication with DMA descriptors and approximately 60 frame IRQs/s | Actual Feather compilation; actual ARM descriptor builder plus a 10-frame DMA/FIFO semantic model | No physical board timing | Private prototype, not part of the validated memory/renderer build |

The instruction counts above execute compiled Cortex-M33 code in Unicorn. They measure instruction work, not elapsed MCU cycles: PSRAM/QMI latency, flash cache effects, pipeline stalls, interrupts, and peripheral contention are absent. They cannot be converted directly into gameplay FPS percentages.

## Memory candidate: what was checked

Ordinary RAM transfers previously fell through generic bus code even though the region and width were known. The candidate adds guarded fixed-width leaves for CPU accesses. Other regions, DMA-sensitive paths, and unsupported cases retain the original fallback.

The candidate preserves guest access cycles, sequential/nonsequential state, high-address mirrors, alignment, read latches, prefetch state, and existing DMA service points. RAM writes retain the original behavior of not changing the open-bus latch. The original RAM write branches have no additional executable-code invalidation hook to bypass. No class fields or memory layout were changed.

Evidence:

- `build/memory-experiments/write-audit.log`: 300,000 randomized fixtures, 262,479 guarded RAM hits; cycles, state, and memory match.
- `build/memory-experiments/audit-multiple.log`: 15 audit groups pass. The private target excludes the optional timer-reference group; this is not a claim of 16 groups.
- `build/memory-experiments/replay-multiple-6000.log`: 6,000-frame CPU, banked-register, timing, bus, memory and pixel parity; final frame hash `5ba30470`, PC `080bc248`. Includes direct RGB565 output, protected margins, and target replacement at frame 3,000.
- `build/memory-experiments/arm-work.json`: exact instruction counts and matching guest endpoints for synthetic single and multiple RAM loops.
- `build/memory-experiments/candidate2-frozen/manifest.json`: frozen source fingerprints and validation manifest. Its private patch and header-overlay instructions remain in the same directory.

The first desktop memory-2 comparison showed CPU-only median time 282.617 to 268.007 ms, but threaded median time 371.681 to 382.382 ms. These runs reached a registration/menu scene and could contend with other agent processes. They are retained as exploratory results, not gameplay or board gains.

## Renderer candidate: what was checked

An observed scene enables a brightness effect with strength zero. The general compositor still did work to locate the second layer even though that layer cannot affect a zero-strength brightness result. The specialized span avoids that work while retaining first-target bit-15 behavior. Translucent OBJ alpha continues through the original compositor where required.

Evidence:

- `build/renderer-experiments/parity.log`: 480 fixtures against the preserved independent v21 renderer, including 240 directed windows/effects cases, cropped/direct RGB565 output, and protected margins.
- `build/renderer-experiments/arm-work.json`: matching microfixture outputs with 47.42–68.04% fewer Cortex-M33 instructions.
- `build/renderer-experiments/candidate-summary.json`: 1,024 directed ARM cases covering 1–4 backgrounds, OBJ on/off, brighten/darken strength zero, all 64 first-target masks, transparency and bit-15 colors.
- `build/renderer-experiments/full-parity.log`: full 6,000-frame parity with final frame hash `5ba30470`, PC `080bc248`.
- `build/renderer-experiments/arm-object-size.json`: PPU executable sections increase from 23,488 to 24,852 bytes; the measured function stack increases from 336 to 344 bytes. Integration must account for this cost.

Exploratory desktop draw-time reductions were 6.36–10.37% in the zero-input window and 8.38–9.67% in the older scripted window. **Those windows are language/registration screens, not confirmed gameplay.** Specialized-path benefit depends on the actual game scene.

## CPU census: what the guest is executing

`build/cpu-experiments/demo-census.json` and `demo-census-result.log` retain an instrumented demo-window census of 9,907,980 guest instructions:

| Guest code location/type | Instructions | Share of counted instructions |
| --- | --- | --- |
| ROM Thumb | 6,316,787 | 63.75% |
| IWRAM ARM | 2,746,177 | 27.72% |
| BIOS ARM | 766,235 | 7.73% |

These are guest instruction counts, not shares of board CPU time. Instrumentation adds host work, so census elapsed time is not a baseline timing result.

The hot IWRAM ARM loop at `0x030002fc`–`0x0300035c` contains signed PCM loads, volume multiplies, and rotate-by-eight mixing accumulators. This is strongly consistent with guest software audio mixing; it is an inference from disassembly, not a symbol-confirmed identification. Disabling host APU synthesis does not stop the game from executing its own mixer.

A private exact-opcode ARM candidate passes 360,000 randomized step states with matching aggregate reference/candidate hash `3732873c5396bb89`, shared pretests, and a 6,000-frame replay. Its exploratory demo desktop result is only about 1.5% faster in compute mode, within timing noise, and about 2% slower threaded. It is not recommended for promotion. Evidence remains in `armhot-benchmark.json`, `armhot-6000-replay.log`, and the associated audit logs.

A separate structural ARM candidate specializes all 12 non-flag-setting data-processing operations for immediate or immediate-shift operands when PC is not involved. It is not tied to exact ROM opcodes. It passes 307,200 randomized state transitions with matching hash `8aad2ef402a6ecaf`, shared pretests, a 6,000-frame replay, and matching demo hashes/cycles. Its synthetic mixer-shaped Cortex-M33 workload falls from 7,193,057 to 6,806,538 instructions, 5.37% less work. A bounded-batch extension also passes replay but takes 6,971,642 instructions, worse than the generic specialization alone, so that extension is rejected. Concurrent desktop runs varied from roughly 208 to 390 ms for the baseline and cannot support a claimed gain. Full decisions, source/input fingerprints, and counts are retained in `build/cpu-experiments/cpu-experiment-report.json`; no board FPS is claimed for either candidate.

The subsequent IWRAM fetch candidate reads the current writable IWRAM bytes for every opcode, rather than caching decoded opcodes. It preserves one-cycle access timing, open-bus and last-access state, mirrors and alignment. It passes 69,264 accepted-read cases and 30,736 side-effect-free rejection cases, shared pretests, a 6,000-frame replay, and demo CPU/cycle/pixel endpoint parity. A synthetic Cortex-M33 IWRAM mixer loop uses 12,667,737 versus 9,846,679 instructions, 22.27% less work. Tentative desktop medians are 215.127 to 202.867 ms compute and 255.482 to 252.857 ms threaded; these are separate local measurements. The later healthy board C capture is reported below. Private source/patches and updated results remain under `build/cpu-experiments/`.

## A test failure that changed the conclusion

The CPU `tail1` polling candidate and its combination with a polling-register change passed 64,000 randomized focused slices. The combined full replay then failed with `FAIL: VBlank wait CPU/banked/IRQ parity` in `build/cpu-experiments/combined-6000-replay.log`.

That candidate is rejected. The exact mechanism is unproven; event-phase exposure is a hypothesis. Its apparent desktop timing benefit is not usable evidence for a correct firmware release. The passing focused logs and failing full replay are both retained so that the failure is visible in the development history. The separate polling-register candidate passes its replay but still needs a stronger audit for register state after an IRQ during a polling loop; it is not recommended for promotion either.

## Corrected scene labels and exact frame selection

Screenshot inspection corrected an earlier benchmarking assumption:

| Input/window | Actual observed scene | Allowed interpretation |
| --- | --- | --- |
| Zero input, warmup 4,500; completed frames 4,501–4,680 | Language selection | Menu test only |
| Older scripted input, warmup 3,600; next 600 frames | Registration/menu | Menu test only |
| A-only language-confirmation pulse | Language/menu remains | Not evidence of gameplay |
| Confirmed demo policy 3, warmup 5,000; completed frames 5,001–5,180 | Attract/demo play window, inspected using host captures | Intended matched gameplay comparison |

Policy 3 uses zero-based guest-frame indices and start-inclusive, end-exclusive five-frame pulses:

| Frames | Key mask | Input |
| --- | --- | --- |
| 300–304 | `0x001` | A |
| 600–604 | `0x001` | A |
| 800–804 | `0x008` | Start |
| 1,000–1,004 | `0x001` | A |
| 1,200–1,204 | `0x008` | Start |
| All other frames | `0x000` | Released |

The host demo replay runs 5,400 frames and passes with final frame hash `cfdcc6d5`, PC `080bc824`; see `GBAPrototype/logs/perf-lab/host-v53-demo-validation/check-2.log`. This validates the policy and emulator parity, not board FPS.

The firmware owns the measured interval, freezes its endpoints, and retains each frame record. `tools/frame_benchmark.py` checks the exact frame numbers, input masks, guest-cycle sum, no overwritten records, and matching CPU/RAM/video endpoint hashes. A comparison also rejects different CPU or PSRAM clocks. USB polling and export happen outside the frozen interval. Repeated matched trials are still needed to quantify timing noise.

## Integrated build and board measurement status

`output/v53-memory-render-language/validation.json` records a passing firmware validation:

- UF2 SHA256: `38db63fa7d691a82a96ce59d0f6e6201a41ba68a6778739bc745ff3d211fc60a`.
- ELF SHA256: `8d6fde6023d6003a30b5ca854b1ea4a32bf7d34622d8385e4272e44f8c354d51`.
- RAM load: 63,956 bytes; preallocation heap: 259,704 bytes.
- Map fingerprint: `2822709056`.

`logs/perf-lab/host-v53-memory-render-validation/checks.json` records passing integrated checks and a 6,000-frame replay. The language-scene build name is deliberate: it must not be relabeled as a gameplay measurement.

| Board experiment | Workload | Current status | FPS result |
| --- | --- | --- | --- |
| `logs/perf-lab/v53-language-baseline-01` | Policy 0; language menu, frames 4,501–4,680 | Completed, healthy, 180 frames, no dropped records | 14.96755 FPS |
| `logs/perf-lab/v53-language-memory-render-01` | Intended same language-menu window | Boot fails: `Cannot allocate GBA interpreter SRAM` | No result |
| Policy-3 gameplay baseline/candidate | Intended demo play, frames 5,001–5,180 | Host policy/replay validated; queued board flow cancelled after boot failure | Not measured |

The baseline was captured on 2026-10-20 at 21:17:14 local time (+01:00), with CPU 240 MHz and PSRAM 30 MHz. Mean timings are Run 63.053 ms, Draw 28.777 ms, Wait 18.611 ms, queue-full 18.497 ms, and barrier 0.114 ms. Run outside renderer waiting is 44.442 ms/frame. These counters overlap: Draw is renderer work, Wait is game-thread waiting within Run, and their values must not be added as independent frame phases. HDMI IRQ rate is 31,499.78/s. Raw capture SHA256 is `f0afece867f319ccf9b9784f9894fc23a5f4d4a70926167ca09999ce2e302399`; frozen endpoint hashes and complete metrics are in `capture-benchmark.json` and `capture-summary.json` in the baseline directory.

The user reported the candidate's HDMI allocation error at 21:23:43 local time; the failure record is `logs/perf-lab/v53-language-memory-render-01/failure.json`. The exact observation is that `malloc(sizeof(Emulator))` returns null before the interpreter constructor runs. The baseline capture had 9,776 bytes of free heap, and the combined candidate adds 2,656 bytes to RAM load. Low headroom/fragmentation is an inference from those observations: the largest free block was not measured, so the allocator root cause is not proven. This is a boot-memory failure, not a measured FPS regression.

No candidate percentage could be inferred from that failed attempt. Build success, instruction counts, and desktop timing alone do not establish a board gain; the later repaired pair is reported separately below. The raw log index records what exists at its snapshot time; in-progress files can change and need a fresh inventory when the run completes.

### Repaired exact-demo baseline

The new policy-3 baseline completed healthy on board at 21:48:54 local time (+01:00): **6.79999 FPS**, frames 5,001–5,180, with Run 139.642 ms, Draw 36.906 ms, Wait 15.473 ms, queue-full 15.122 ms, barrier 0.351 ms, and Run outside waiting 124.168 ms/frame. Minimum free heap is 17,400 bytes. This proves the repaired baseline boots.

Evidence is `logs/perf-lab/v53-repaired-demo-sequence-01/baseline/capture-benchmark.json` and `capture-summary.json`. Capture SHA256 is `5813ce524fc2885a07edc52be8259a8134f87d6a296929ff35672dd30ec5d60c`. Guest cycles are exactly `1404396939` to `1454957734`, matching the host access-census interval. CPU/RAM endpoint hashes are `1472993050` to `1806797606`, and video endpoint hashes `2050365287` to `2537336287`. CPU/PSRAM clocks remain 240/30 MHz. These endpoints define the work for comparison; timing counters overlap as described above.

### Completed repaired A/B pair

The memory/renderer candidate completed healthy at 21:58:41 local time, with every start/end CPU/RAM/video hash and guest-cycle endpoint exactly matching the baseline:

| Metric | Repaired baseline A | Memory/renderer B |
| --- | ---: | ---: |
| Weighted FPS | 6.79999 | 7.03893 |
| Run ms/frame | 139.64153 | 134.84056 |
| Draw ms/frame | 36.90564 | 32.21521 |
| Wait ms/frame | 15.47343 | 13.02359 |
| Run outside wait ms/frame | 124.16810 | 121.81696 |

This pair gains **0.23894 FPS, or 3.51381%**. It measures the combined memory/renderer change; it does not isolate each contribution or establish repeated-trial stability. The large remaining Run time also shows that this improvement is far from a 60 FPS result.

The comparison is retained in `logs/perf-lab/v53-repaired-demo-sequence-01/sequence.json`; raw/decoded candidate data is in its `candidate/` subdirectory. Candidate raw capture SHA256 is `091b64c21f468a82f57b51cf9e34f9f50d56f93164d3b80917e74b9dfdad9cfb`, and immutable `candidate/capture-benchmark.json` metadata SHA256 is `e623b3915fb98a32e36a389b2c9fbed8f0f17d641ef410e45125dd3ce59b6f3b`. V52 restoration was started after the pair, before the follow-up C/D sequence at `logs/perf-lab/v53-followup-demo-sequence-01`, described below.

### Follow-up C: IWRAM fetch leaf

The IWRAM variant C completes healthy at 22:11:18 local time with **7.26469 FPS**, versus B's 7.03893 (**+3.2073%**) and A's 6.79999 (**+6.8338% overall**). Run is 130.663 ms, Draw 32.400 ms, Wait 13.207 ms, and Run outside waiting 117.456 ms/frame. All CPU/RAM/video and guest-cycle endpoints match A/B exactly. This is one capture per variant; a repeated/interleaved trial is still needed to establish stability.

Evidence is `logs/perf-lab/v53-followup-demo-sequence-01/iwram/capture-benchmark.json` and its summary, with raw capture SHA256 `0fa9aa67e329b21f14691ff32b7b7435783fe25f5d10f547bd4b035bd9fb7978` and metadata SHA256 `6193b110e5e533048c02c448a2e637a8f211eb4cd2d9f8ec260d776e650f05fc`. The sequence has moved to compact-output D; D has no measured result here. Pin-only E is being prepared separately from timing instrumentation.

## Automatic matched-test sequence

The queued board sequence is recorded in `logs/perf-lab/v53-matched-sequence-01/sequence.json`, with process output retained in `build/v53-continued-board-tests.log`. It started at 2026-10-20 21:21:13 local time (+01:00), then changed to `cancelled-after-language-candidate-boot-failure`.

The planned sequence was: finish the language-menu candidate capture, compare exact frozen endpoints, flash/capture policy-3 demo baseline and candidate for completed frames 5,001–5,180, then restore V52 with normal input. After the candidate boot failure, the verified capture/uploader processes were stopped and the demo steps cancelled. A separate known-V52 restoration is recorded in `build/v53-restore-v52.log` and confirmed responsive at 21:27:13 local time. The failure record now includes `restored_v52: true` and video status. No demo A/B comparison completed.

Both demo images already pass firmware validation, using isolated source snapshots rather than attributing the build to a changing working tree:

| Demo image | Source snapshot | RAM load | Preallocation heap | Validation manifest |
| --- | --- | --- | --- | --- |
| Baseline | `build/v53-demo-baseline-source/GBAFeather` | 61,300 bytes | 262,360 bytes | `output/v53-demo-baseline/validation.json` |
| Memory/renderer candidate | `build/v53-demo-candidate-source/GBAFeather` | 63,956 bytes | 259,704 bytes | `output/v53-demo-memory-render/validation.json` |

The manifests retain actual snapshot fingerprints and ELF/UF2 hashes; the evidence index also hashes the artifacts. The candidate demo UF2 hash is `3a22083df97a18f4ef5956e3273dc6dac9e0745be85b24184e5937dfca6e472d`; baseline is `9d3f567d6fce367c5997c47ec10c2c874932cc551cc89be4a57e04a73065991b`. These validated images are inputs to the queued experiment, not evidence that the board captures have completed.

## HSTX DMA prototype: separate opportunity

The existing output driver performs scheduling and source preparation in approximately 31,500 line interrupts per second. HSTX already handles TMDS encoding. The prototype uses narrow DMA writes for horizontal pixel duplication and a static three-channel descriptor ring for headers, pixels, vertical duplication, and frame rewind. It aims to keep output progressing without per-line CPU service.

`build/renderer-experiments/hstx-model.json` reports a passing actual-ARM-builder/semantic-model test: ten autonomous frames, 1,005 descriptors, 157,230 FIFO words per frame, all 65,536 RGB565 colors, all 525 lines, and exact header/sync/duplication sequences. Storage is 16,160 bytes of internal SRAM. `hstx-compile.log` records Feather-target compilation.

This does not establish physical DMA arbitration, FIFO starvation tolerance, monitor acceptance, output stability, or a performance gain. The prototype is not part of the memory/renderer firmware above. Its integration requires an isolated driver fork, initialization before DMA starts, correct reset/recovery handling, and the existing scanout fallback. Detailed private notes are retained in `build/renderer-experiments/HSTX-INTEGRATION.md`.

Debug-log support now reserves flag `1024` for frame-IRQ scanout and checks the corresponding 45–75 IRQ/s health range; ordinary scanline mode retains its line-rate health check. Sixteen support/health tests pass in `build/v53-hstx-health-tests.log`. A separate sandbox-denied attempt is retained as execution-environment evidence, not a functional test failure. This support has not yet been built and measured as a physical HSTX-ring firmware trial.

The C++ monitor pretests and a one-frame replay also pass in `build/v53-hstx-monitor-tests.log`, with build output in `build/v53-hstx-monitor-build.log`. This verifies monitor support locally; it is not a physical scanout trial.

### Compact frame-IRQ variant

A separate compact HSTX model reduces storage to 6,880 bytes, avoids 2,616 legacy line-buffer bytes, and has a modeled net RAM increase of 4,264 bytes using four DMA channels. It saves 9,280 bytes versus the autonomous three-channel ring above. Ten actual-ARM-builder/model frames match the original FIFO stream, including all 65,536 colors and 525 lines; see `build/renderer-experiments/hstx-compact-model.json`.

The compact variant needs a CPU frame interrupt to restart DMA. It is not autonomous when that interrupt is absent. Its modeled conservative restart deadline is 27.30 microseconds; cases at 28/50/100 microseconds exceed that worst-case allowance. This is a semantic deadline analysis, not a measured interrupt latency or proof of physical FIFO starvation. Physical stress testing remains necessary. The compact candidate firmware passes validation in `output/v53-compact-demo-candidate/validation.json` with UF2 SHA256 `bcbba91f93ead7eb5db2b749158167b8c2ceb793e84a988a9448d5a6c4894f25`; no board FPS result is claimed here.

## Exact demo memory-access census

`build/memory-experiments/access-census/` contains a frozen reference/instrumented desktop experiment for policy 3 and completed frames 5,001–5,180. All 181 checkpoints, including the starting state, match CPU banks/SPSRs, cycles, memory, bus/IRQ state and rendered pixels. An independent CPU fetch-entry meter exactly matches the 9,907,980 recorded fetch operations. The focused audit verifies successful/rejected leaves, delegated fallback counting and DMA raw-copy coverage. Raw traces, source/input fingerprints, reports and SHA256s remain in that directory and the experiment index.

| Actor | Operations across 180 frames |
| --- | ---: |
| Code fetch | 9,907,980 |
| CPU data read | 1,671,711 |
| CPU data write | 866,772 |
| DMA read | 856,732 |
| DMA write | 856,732 |
| Host-only peek | 25,642 |

EWRAM accounts for 275,906 operations, 1.945% of the total: 154,072 CPU reads, 105,834 CPU writes and 16,000 DMA reads, with no DMA writes in this window. CPU EWRAM averages 856 reads and 588 writes per frame, carrying about 4.49 kB of logical read/write payload combined. Twenty 1 KiB pages are touched; five pages account for 79.84% of EWRAM activity. Strong alternating strides are consistent with the paired-buffer pattern seen in the mixer disassembly. This does not identify all accesses by game function.

A modeled 2 KiB/64-byte/two-way read cache hits 34.87% when stores invalidate, or 87.55% when CPU stores update existing cached lines. The latter still fetches 1,227,776 line-fill bytes versus 426,870 direct CPU-read payload bytes, requires 89,615 cached-write updates, and retains PSRAM write-through stores. Tag, copy, coherence and SRAM costs are not timed in the model. A broad EWRAM cache is therefore not established as a major speedup.

The more substantial semantic copy traffic is in the ROM pipeline: **69,826 PSRAM-to-hot-cache fills totaling 17,875,456 bytes**, and **234,836 SRAM hot-cache-to-stable-instruction-window copies totaling 60,118,016 bytes** across the same window. Only three backing pages are decoded, emitting 49,152 bytes. This is roughly 99.3 kB external ROM-copy payload and 334 kB internal window copying per frame. Low decompression counters therefore do not imply low cache/window traffic.

These are logical operations and bytes passed to copy/decode routines, **not physical memory transactions, bandwidth measurements, cycle shares or FPS projections**. Renderer accesses, decompressor/CRC rereads and hardware cache/bus behavior are not expanded. The dependent pointer-chase PSRAM timing must not be multiplied by every logical access. Actual per-fill/per-window-copy timings are a next measurement. A direct borrowed hot-cache pointer needs a lifetime/pin/generation guarantee because data/DMA ROM reads can evict the current code line.

### Pinned code-window candidate and separate timing hook

The provider contract permits a code-line pointer to remain valid until its next code-line call. A private original-support candidate pins the current SRAM hot-cache slot and makes conflicting data fills evict the other way. It releases/rebinds the pin on the next code-line request. This removes the copied window while preserving its lifetime guarantee; one of 64 two-way sets temporarily has one replaceable data way. No third-party CPU/Bus/Cartridge source is changed by this candidate.

The held pointer survives 65,536 adversarial same-set/cross-line reads, prefetch/bulk traffic, 256 rebinds and invalid-address rejection. All 181 demo checkpoints, logical bus counts and the raw EWRAM trace match the reference. The candidate removes all 234,836 window-copy calls/60,118,016 copied bytes, while this scene retains exactly 69,826 hot fills and three backing misses. Other conflict patterns can behave differently. Actual Feather provider layout saves 252 SRAM bytes, from 5,120 to 4,868.

The frozen handoff is `build/memory-experiments/rom-window-study/README.md`, `report.json`, `manifest.json`, and `pin-only.patch`. Production header SHA256 is `ab6d7a67f1c030c19fc529859ff0d048ce30edc8b503ed420b2fb270c9952e11`; report SHA256 is `57ec631de93cd797ff6028f4af2764b684fc9bf79a130369e24c19f5b84aee8b`. A first E preflight selected the instrumented research header and correctly failed on its missing census-hook dependency; that packaging error is retained in `build/v53-pinned-demo-instrumented-header-failed.log`. The corrected build uses the production header. An initial read-only patch check also exposed Windows-translated patch line endings; normalization to LF fixes ordinary `git apply --check`, with both results retained. Neither is evidence that the production algorithm fails.

Separate optional original timing support uses `time_us_32()` only around hot-fill work and the old window memcpy, with cumulative USB-readable counters and 72 bytes of state when enabled. Disabled runtime support reads no clock; compile-time exclusion removes the hooks entirely. Its 100,000 deterministic timestamp samples, wrap/zero-duration/failure accounting and actual no-exceptions target compilation pass. Measurements will include interrupt preemption and timer overhead. The pin-only FPS trial contains no timing hooks; these counters have no board-time result yet and require no binary log-schema change.

## Flash AOT feasibility, separately measured

The CPU agent's two-pass Flash AOT prototype executes actual Cortex-M33 payload code under Unicorn, with stable payload/metadata/gate addresses across both links and no runtime emitter symbol. Synthetic ALU work falls from 11,379,769 to 7,422,287 host instructions (34.78% less); a mixed RAM fixture falls from 11,472,874 to 8,495,195 (25.95% less). Register/CPSR/cycle results match; runtime compilation, mismatches and Flash writes are zero. An unknown-ROM binding disables the backend and matches interpreter work exactly.

The synthetic payload occupies 3,208 bytes in a fixed 16 KiB reservation with 1,408 metadata bytes. A 16-slot SRAM lookup cache plus state/metrics adds roughly 172 bytes while the existing 4 KiB execution union is still allocated. Those fixtures fit the old JIT cache and do not demonstrate a Sonic cache-thrashing or gameplay gain. Real ROM binding, lookup-miss handling, catalog coverage, XIP contention and board timing remain separate work. Evidence is `build/cpu-experiments/aotprototype/prototype-report.json` and `link-verification.json`; no Sonic or board FPS gain is inferred from these synthetic reductions.

## What the evidence supports next

### Compact DMA rejection and safe continuation

During the physical compact HSTX DMA trial the user reported **flicker or
corruption**. Root stopped only the verified C/D capture process (PID 23284),
retained the current ring, and performed one serial-specific V52 restoration.
The restoration completed at **22:19:37 local time on 20 October 2026** and USB
telemetry responded. The failed ring also reports HDMI restarts; it is a warmup
capture, not a matched gameplay FPS result. No compact-driver speedup is accepted.
The semantic FIFO/DMA model did not prove adequate physical frame-restart slack.

Evidence is `logs/perf-lab/v53-followup-demo-sequence-01/sequence.json`,
`compact/before-fallback*`, `restore-v52-after-visual-failure/`, and
`build/v53-compact-visual-failure-restore.log`. The first pending pinning job
correctly stopped without touching USB when its predecessor failed. A second
job was explicitly started after fallback verification, using the established
legacy HDMI driver and its own OS board lock. It is
`logs/perf-lab/v53-pinned-demo-sequence-02/`. Its healthy matched capture later
measures **7.10071 FPS versus B's 7.03893 FPS**, a **0.88%** increase in one trial.
All CPU/RAM/video endpoint hashes and guest cycles match. Run outside waiting
falls 121.817 to 120.668 ms/frame; Draw remains essentially unchanged, 32.215 to
32.225 ms/frame. This small difference needs repeats and does not establish
internal copying as the main performance limit. V52 restoration completes before
the next queued Flash-AOT test receives the OS lock.

### Real Sonic Flash translation prepared for board measurement

The larger experiment now contains actual Sonic ROM blocks, rather than only
the earlier synthetic AOT fixture. Catalogs of 128 and 512 Thumb blocks pass all
181 checkpoints over the same 5,000-frame interpreter warmup and 180-frame
gameplay interval, in both semantic-model and actual Cortex-M33 machine-code
replays. These compare CPU/banked registers, cycles, bus fields, RAM, native
pixels, IRQ, DMA, timers, PPU and APU state. They establish correctness in this
workload, not real-board performance.

The 128-block replay executes 607,503 translated batches/2,439,157 translated
instructions, roughly 24.6% of all guest instructions in the interval. The
512-block replay executes 869,228 batches/3,467,564 translated instructions.
Both compile zero blocks at runtime and report zero mismatches. ARM code,
uncatalogued Thumb code and BIOS code still use the interpreter.

The first actual Feather build uses a fixed 64 KiB Flash payload reservation
(35,968 bytes emitted code), runtime SHA256 streamed from each decompressed,
CRC-validated ROM page, and an explicit ROM-size/hash binding. Unknown ROMs
retain the interpreter. It warms up with the interpreter and enables translation
only at the measured boundary. Two-pass linking resolves the real final helper
addresses, retains otherwise unreachable helper symbols, and verifies unchanged
symbol addresses/sizes plus every payload/hash/catalog byte. A final PC-map
check and ordinary UF2/ROM/BIOS/stack checks pass. Placeholder payloads cannot
be delivered by the verified builder.

Its artifact is `output/v53-aot128-demo-candidate/`, with final-ELF proof in
`build/aot-v53-aot128-demo/final-verification.json`. An initial Arduino compile
failed because duplicate slash/backslash include aliases bypassed `#pragma once`;
the redundant original-support include was removed, and the failed log remains
`build/v53-aot128-demo-first-include-failed.log`. The actual corrected build
passes. Raw PC samples inside translated Flash bytes map to a stable ELF marker.
Capture tool 0.1.1 requires Flash-AOT identity flags and post-freeze proof of
positive translated execution, zero dynamic compilations/mismatches, and a still
valid bound catalog. Five capture/parser tests pass.

The first actual board measurement subsequently completes at **5.47270 FPS**,
versus C's **7.26469 FPS**, a **24.67% regression**. All guest endpoints/cycles
match, and post-freeze status reports exactly 607,503 batches, 2,439,157 translated
instructions, zero dynamic compilations, zero mismatches and a valid catalog.
Run outside renderer waiting rises from 117.456 to 159.535 ms/frame, while Draw
is 32.096 and Wait 13.028 ms/frame. This baseline is not promoted. Its capture,
native/catalog execution proof and fallback are retained under
`logs/perf-lab/v53-aot128-demo-sequence-01/`.

Complete-window board PC samples include generic experimental dispatch 22.96%,
AOT lookup 6.40%, Bus read 6.22%, ARM execute 6.11%, native gate 5.48%, native
admission 5.46% and translated body 3.32%. These are exclusive sample shares;
the C++ gate fixture's approximately 31% share is not substituted for real-board
time. An independent audit finds an avoidable placement change: the ordinary
interpreter loop is in SRAM (C address `0x20006e54`, 2,608 bytes), while activating
native execution selects the generic experimental loop in Flash (AOT address
`0x100117d0`, 4,286 bytes). Most guest instructions remain untranslated. This
proves the placement difference, not how much of the regression it causes.
A dedicated native-only slice with separate Flash/SRAM controls is being checked.

The negative bitmap lookup variant passes complete semantic/M33 replays and its
actual Feather two-pass ELF/payload checks. It adds 256 Flash bytes and no SRAM
for 128 entries. Its original pending board job was postponed before any USB
access to prioritize the newly identified CPU-loop placement fix. A separate
Flash-AOT-only cache specialization recovers 3,328 SRAM bytes, with legacy code
unchanged and both catalogs preserving all 181 replay checkpoints. This may fund
the dedicated loop or autonomous HDMI; it is not yet integrated in these captured
builds. The private stack-watermark helper also passes tests, but the 8 KiB core-1
stack is retained until actual high-water observations support a change.
The combined dedicated-loop/cache-reclaim source passes 6,000-frame policy-3
replay and 48,000 focused timing/event slices against the original interpreter,
including all 1,001 native-active checkpoints and exact native counts. Its actual
final ELF places the 2,592-byte native loop at `0x200078a4` in SRAM. Added
SRAM-loaded code is 2,548 bytes versus the first AOT image; class storage shrinks
3,328 bytes, leaving approximately 780 bytes net headroom. Final map, payload,
stack and private asset checks pass. The controlled board job is
`logs/perf-lab/v53-native-slice-demo-sequence-01/`.

That board capture subsequently completes at **5.89174 FPS**, with identical
CPU/RAM/video endpoint hashes, guest cycles and native execution counts. This is
**7.66% above the original AOT build**, but still **18.90% below interpreter C**;
it is not promoted. Run outside renderer waiting falls from 159.535 to
147.100 ms/frame, still above C's 117.456. Draw is 32.168 and Wait 13.087 ms/frame.
The bitmap, loop specialization, placement and storage changes were combined,
so this first result does not isolate their individual effects.

Its 12,226 eligible board PC samples put the native-only loop at 20.86%, native
admission at 5.87%, per-op native gate at 5.80%, AOT lookup at 4.57%, ARM execution
at 6.42% and translated bodies at 3.14%. Samples are exclusive function shares,
not additive inclusive call-tree costs. Native execution remains 607,503 batches,
2,439,157 instructions and 658,549 memory operations, with zero mismatches or
dynamic compilations. The next queued control places the identical 2,592-byte
native-only loop in Flash at `0x10011ac8`, keeping class storage and the negative
bitmap identical. It also checks that BIOS placement matches, to avoid confusing
code-placement effects with a changed BIOS allocation. Its evidence folder is
`logs/perf-lab/v53-native-flash-demo-sequence-01/`; no Flash-control FPS yet.

The matched Flash control subsequently completes at **5.49715 FPS**, versus
**5.89174 FPS** for the same specialized loop in SRAM (about **7.18% higher FPS
in SRAM**). All CPU/RAM/video/cycle endpoints and native execution counters
match; both builds retain the BIOS in Flash. Run outside waiting is 158.755
versus 147.100 ms/frame. This isolates placement far better than comparing the
first general experimental loop with the combined specialization/filter build.
It supports retaining SRAM placement, but does not explain away native
execution's remaining regression against interpreter C.

The next CPU candidate moves the catalog's definite-negative test into the SRAM
caller, before the Flash admission function. A possible positive still follows
all original ROM identity, generation, opcode and exact-catalog checks. It
preserves the complete 6,000-frame model replay and 48,000 focused event slices;
the separate actual generated-M33 replay matches all 181 demo checkpoints and
the board-compatible native execution counts. Admission requests fall from
3,430,642 to 777,344 in that replay. Lookup/cache diagnostics intentionally change;
native instructions, batches, memory operations, exits and mismatches do not.

Actual firmware ELF/UF2/map/asset/stack validation passes for
`output/v53-early-admission-demo-candidate/`. SRAM-loaded code grows by 88 bytes
versus the native-only SRAM build, with 255,840 bytes of preallocation heap.
This is preallocation space, not runtime free heap. The job queues behind the
placement control and restores V52 after capture. No FPS gain is claimed yet.
Evidence is `logs/perf-lab/v53-early-admission-demo-sequence-01/` and the private
`build/cpu-architecture/early-admission-report.json`.

Two preparation failures were caught before upload and retained: the first
invocation selected an older emitter with an incompatible CLI, and the first
post-build validation ran before the generated PC map was copied to the delivery
folder. The corrected catalog helper, final two-pass payload verification and
matching-map validation all pass; neither failure touched the board.

The first early-admission board capture reaches frame 5,180 with all frozen
CPU/RAM/video/cycle endpoints matching, but HDMI restarts at frame 5,148 (restart
counter 3 to 4). Health validation rejects it before post-freeze native proof.
Its raw 6.216 FPS reading is **not a valid clean comparison and is not promoted**.
The raw/decoded capture remains under
`logs/perf-lab/v53-early-admission-demo-sequence-01/`. V52 restoration and live
video status are verified at 23:36:34 local. The conservative queue prevents the
first ARM/pair jobs from touching USB after the rejected run.

After reviewing that failure and the verified fallback, a new explicit queue
tests the independent F-based ARM candidate first, then makes one clean repeat
of early admission, then tests bounded fetches. New folders end in
`v53-arm-native-demo-sequence-02`, `v53-early-admission-demo-sequence-02` and
`v53-fetch-pair-demo-sequence-02`. Each holds the same serial-specific lock,
requires its predecessor's verified restoration, and restores V52 afterward.
Only the independent ARM job may proceed after the specifically reviewed prior
HDMI rejection; subsequent failures halt the chain. No error is relabeled as a
successful performance result.

The user renewed the request for multiple CPU agents. Three independent tasks
now cover native Thumb gates, writable IWRAM ARM native blocks, and interpreter/
event dispatch. A research review also considered
[GBA-Recomp's C11 translation and RAM-code capture](https://github.com/JRickey/gba-recomp).
Its block-edge timing/IRQ assumptions are not imported into this exact-transition
oracle; native C/C++ block generation remains a potential implementation approach,
not evidence of RP2350 speed or compatibility.

The independent CPU census counts 9,907,980 executed guest instructions across
the 180-frame demo window: 6,395,298 Thumb and 3,512,682 ARM instructions. The
ordinary loop's 32.21% board sample share includes inlined fetch and ALU work;
it must not be described as removable scheduling overhead. Timer service,
hardware service and schedule refresh together account for only 2.41% of
exclusive samples in C. Even removing all of that work would leave most of the
performance gap. A useful next translation must remove repeated decode, fetch
and callback work while retaining event boundaries, rather than merely changing
the dispatch label or skipping scheduled guest execution.

At 240 MHz, the 60 FPS target allows four million system-clock cycles per frame.
This workload executes about 55,044 guest instructions per frame, leaving about
72.7 system-clock cycles per guest instruction for all work in the frame. That
is an arithmetic budget, not an instruction-latency measurement; interrupts,
stalls and rendering also consume time. F still spends 117.356 ms/frame outside
renderer waiting. The agents therefore test independent changes to native block
coverage, bounded fetch transitions and a writable-IWRAM ARM sequence. Their
synthetic M33 instruction counts establish reduced computation in selected
paths; only the serial-pinned board capture establishes an FPS result.

The independent ARM candidate is now built on F's ordinary interpreter and
resolved-color renderer, without ROM Flash AOT. It recognizes a nine-instruction
arithmetic sequence in actual writable IWRAM, rechecks mutable code, and uses a
bounded fast lane only when countdown and upcoming hardware deadlines permit.
It retains scalar/fetched-pending fallback. Its exact SDK machine code passes
4,560 mutation/event/ABI cases and all 181 full gameplay checkpoints. The final
firmware's 820-byte SRAM flat body is byte-identical to that tested payload.
Activation occurs only after the 5,000 interpreter warmup frames; aggregate
execution counters can be queried only after the benchmark freezes.

Final validation of `output/v53-arm-native-demo-candidate/` passes. Preallocation
heap is 252,612 bytes, 2,632 bytes below F, before runtime allocations. Expected
180-frame proof is 113,506 entries and 759,307 native guest instructions,
including 52,867 full-fast calls and 475,803 full-fast instructions, with zero
changed-code or pending exits on this particular demo. The native guest coverage
is 7.66%; this small sequence alone cannot plausibly close the overall FPS gap.
The board job queues behind early admission and compares directly with F,
including guest endpoints, BIOS placement and execution counts. It restores V52
afterward. At this preparation stage no board FPS was available; the completed
7.54547 FPS arithmetic result is recorded in the CPU follow-up above. Frozen source/proofs remain
private under `build/memory-experiments/iwram-native-study/`.

The separate generic fetch candidate combines early admission with an exact
bounded prefetch transition, reducing repeated Bus/CPU updates for eligible
immutable pure-ALU prefixes. Individual 128/512 catalogs match all 181 actual-M33
checkpoints, and 250,000 ordinary-Bus differential cases pass. Its synthetic M33
work falls 11.08% for an ALU loop and 3.69% for mixed RAM work. Coverage is only
8.15% of all guest instructions with the current 128-entry catalog, so wider ISA
support and ARM translation remain necessary research directions. The combined
source is separately replayed before any board upload. Neither local work count
is a predicted board FPS percentage.

The exact combined-source replay subsequently passes all 181 checkpoints and
board-compatible native counts, including 777,344 early-admission requests.
Consumed core/header hashes remain unchanged. Generated payload/profile files
changed during the root firmware link; the host harness does not consume them,
and both their initial and final hashes are explicitly retained. The first replay
completed all frames but its optional report exporter failed; the corrected
report-only rerun passes and the earlier failure remains in the evidence folder.
Final firmware validation passes with a 2,888-byte SRAM gate and an 868-byte
outlined Flash Bus helper, plus its SRAM veneer. This costs 288 preallocation
heap bytes relative to early admission. The job queues behind the ARM trial.
Evidence is `build/cpu-experiments/aot-fetch-combined/combined-proof.json` and
`logs/perf-lab/v53-fetch-pair-demo-sequence-01/`; this preparation preceded the
completed 6.25607 FPS attempt-02 result recorded above.

### Resolved-color cache and fused renderer prepared

A fused-only prototype was rejected locally: Cortex-M33 instruction work rose
2.59% and host time was neutral. The stronger candidate stores resolved RGB555
tile rows, reduces cache entries from 2,048 to 1,024, skips entirely transparent
rows, and fuses eligible mode-0 background output with composition. General
effects/window paths remain available. It passes 960 independent PPU fixtures
and the complete 6,000-frame input replay; the integrated IWRAM-fetch/renderer
image also passes the 5,400-frame CPU/cycle/native-pixel replay with final hash
`cfdcc6d5`, PC `080bc824`. Its first wrapper check incorrectly expected a leading
zero in the printed PC; numeric validation fixes that assertion, and both logs
are retained. No emulator-state mismatch occurred in that run.

Alternating desktop demo trials measure 26.50% lower Draw and 6.03% lower total
elapsed time; an actual M33 static-scene replay executes 15.42% fewer instructions.
Neither is a board FPS projection. The PPU object shrinks 8,192 bytes; the actual
integrated ELF adds 3,824 bytes of SRAM-loaded code, leaving about 4,368 bytes of
net headroom before runtime alignment effects. The established HDMI backend is
used. `output/v53-colorfused-demo-candidate/validation.json` passes, and
`logs/perf-lab/v53-colorfused-demo-sequence-01/` waits behind the AOT trial.
Palette-heavy scenes and other-ROM performance remain unmeasured.

The actual F board capture subsequently measures **7.49120 FPS versus C's
7.26469 FPS** (**+3.12%**, or **+10.16% versus repaired A**). Guest endpoints and
cycles match; telemetry is healthy and no records are lost. Draw falls to
24.411 ms/frame and Wait to 9.332 ms/frame. Crucially, Run outside renderer
waiting remains 117.356 ms/frame versus C's 117.456: the game-thread bottleneck
persists even after a useful rendering reduction. Measured cached free heap is
19,032 bytes. This supports the CPU priority, not a claim that rendering alone
can reach 60 FPS. The dedicated CPU-loop job follows F's verified restoration.

An early-reservation change requests interpreter storage before UART/display allocations while leaving construction at stage 35. Combined with the APU storage repair below, the later paired builds allocate and complete successfully. The effects of early reservation and reduced object size were not isolated, so this does not prove the original allocator root cause. Queryable USB `BOOTFAIL` reporting is also implemented, with four parser tests passing, to make future capture failures report an error instead of waiting for manual HDMI transcription.

An inline APU host-output buffer occupies 8,192 bytes even with synthesis disabled. A private RP2350-only PSRAM relocation is now validated locally; renderer tile cache and hot scratch remain internal. The complete handoff is `build/memory-experiments/apu-psram/README.md`, `apu-psram.patch`, and `manifest.json`.

Actual Feather-target layout changes APU size from 8,456 to 272 bytes and Emulator size from 86,944 to 78,760: **8,184 bytes of internal SRAM recovered**, including alignment effects. The board path allocates its 4,096 signed-16 PCM samples once from the existing PSRAM arena, with no internal fallback. Desktop retains its array; `GBA_APU_INTERNAL_MIX_BUFFER` provides a compile-time internal-buffer fallback. Guest synthesis, register, FIFO, event and CPU code are unchanged. Copy/assignment retains independent PCM storage. Allocation failure throws with exceptions enabled and uses the existing abort-capture path under the board's actual `-fno-exceptions` setting.

The original 440-page backing-ROM allocation would leave only 7,168 PSRAM bytes before adding the new 8,192-byte PCM buffer. The paired budget change reserves one 16 KiB page, leaving **439 evictable backing pages**, without reducing maximum 128 KiB cartridge save capacity. All 5,040 allocation-order permutations fit backing ROM, video backup, GBA framebuffer, EWRAM, full save, debug log and PCM in 7,848,960 of 7,864,320 arena bytes, leaving 15,360. The old plan correctly fails the audio allocation. Apply this budget identically to matched baseline/candidate builds.

Baseline, external-PSRAM, internal-fallback and desktop fixtures all pass 100,000 randomized audible/silent APU event steps, with identical PCM hash `88cd8cab7602d5fa`, 100,420 samples and 87,542 nonzero samples. They check register/FIFO/deadline parity, saturated capacity, deep copy/assignment, placement, fixed allocations and failure behavior. Actual Feather APU-constructor, Emulator-constructor and APU implementation compilation also passes with exceptions disabled. The complete repaired baseline and memory/renderer candidate subsequently boot and finish the healthy matched board pair above. The common allocation repair is not independently timed as a speedup.

The first integrated memory-repair suite failed the existing arena fixture at `Complete ROM cache and 128 KiB save arena budget`; its hardcoded expected size was 7,832,576 bytes and its allocation scenario omitted PCM and debug storage. Core audits passed in that run. This is recorded as a test-maintenance failure after changing the allocation plan, not as proof that the new worst-case budget fits. The old failed run remains in `logs/perf-lab/host-v53-memory-repair-demo/`.

The fixture was updated to actually allocate PCM, debug storage and the full save, then assert 7,848,960 bytes used and 15,360 bytes remaining. The rerun passes shared audits and the complete policy-3 5,400-frame CPU/cycle/pixel replay with hash `cfdcc6d5`, PC `080bc824`. Evidence is `logs/perf-lab/host-v53-memory-repair-demo-02/checks.json` and `check-4.log`, plus `build/v53-memory-repair-checks-02.log`; checked at 21:35:39 local time, source fingerprint `7f5020ff12ac44a6c314099c050db23b588d72ef626d96e42c792e5eac4e23f2`. The later healthy matched board pair demonstrates allocation success for these builds; it does not establish unlimited headroom for further changes.

Retain every raw capture and decoded report, and compare the confirmed demo-play interval before claiming a gain. Promote only candidates that preserve endpoints and improve repeated board results. The current evidence supports less work in selected RAM and compositor paths; it does not establish that those paths are the largest remaining gameplay bottleneck or that 60 FPS is achieved.
