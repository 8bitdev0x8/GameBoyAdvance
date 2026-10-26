# Why CPU improvements have not produced a large FPS jump

Updated 21 October 2026, Dublin. This records the fixed Sonic demo workload,
completed guest frames 5001–5180 with input policy 3. Earlier menus, user-reported
instantaneous FPS and synthetic tests are not interchangeable benchmarks.

## Measured position

| Board candidate | Weighted FPS | Meaning |
| --- | ---: | --- |
| Repaired V53 baseline A | 6.800 | Exact-frame reference |
| IWRAM-fetch interpreter C | 7.265 | Faster fetch path; normal interpreter |
| Resolved-color renderer F | 7.491 | +10.16% versus A in first matched trials |
| First ROM Flash AOT128 | 5.473 | Legacy endpoint checks pass; translated execution is slower |
| Native-only loop in SRAM | 5.892 | Recovers some native-path cost; still below C |
| Identical native-only loop in Flash | 5.497 | Placement control; SRAM is about 7.18% faster |
| Early catalog-negative admission | 6.293 | Fewer costly catalog calls, still below interpreter C |
| Early admission + bounded fetch pairs | 6.256 | Exact execution/endpoints, still slower than interpreter C |
| 4,096-entry native catalog | 4.972 | 54.91% native instruction coverage; −31.55% versus C |
| F + guarded native ARM arithmetic | 7.545 | +0.72% versus F; one trial, noise unmeasured |
| F + typed native ARM extension | 7.622 | +1.74734% versus F; one sample, not statistical proof |

These are first completed captures, not repeated-trial confidence intervals.
The [version record](PERFORMANCE-v53.md) retains hashes, failures, validation and
the serial-specific upload/capture/fallback sequence. V52 remains the normal
controller-input recovery firmware.

## The remaining gap

The typed ARM trial still takes **115.04 ms/frame outside renderer waiting**,
versus F's 117.36 ms/frame. Its measured Run is 124.576 ms, Draw 24.615 ms and
Wait 9.532 ms per frame. All strict legacy CPU/RAM/video endpoints and guest
cycles pass; the result is one observation, not a demonstrated repeatable gain. A
60 FPS frame has only **16.67 ms for everything**. F's renderer reduction helps,
but leaves that game-thread time essentially unchanged. Run includes interrupts
and memory stalls; it is not a measurement of arithmetic alone. Draw, Run and
Wait overlap and must not be summed as independent costs.

The workload executes 9,907,980 guest instructions in 180 frames: about 55,044
per frame. At 240 MHz and 60 FPS, the overall budget is about 72.7 system-clock
cycles per guest instruction. Rendering, interrupts and stalls also use that
budget. This arithmetic does not establish that 60 FPS is achievable.

Ordinary execution still performs opcode fetch, guest bus/prefetch bookkeeping,
decode, register/flag operations and hardware-boundary checks. The present
128-entry native catalog translates only **24.62% of executed guest instructions**.
It still interprets 39.93% as Thumb and 35.45% as ARM. Its 607,503 native entries
execute 2,439,157 guest instructions: roughly four guest instructions per entry.
Short blocks and C++ timing/memory callbacks can cost more than the arithmetic
translation saves. That is why the native baseline regresses despite executing
real translated machine code.

## Current independent CPU experiments

| Track | Change being evaluated | Evidence available | Still required |
| --- | --- | --- | --- |
| Admission | Reject definite catalog misses in the SRAM caller | Exact native counts and CPU/RAM/video endpoints; 6.293 FPS in healthy second attempt | Regresses versus C; first attempt rejected for HDMI restart |
| Fetch batching | Commit bounded pure-ALU bus/CPU fetch transitions together | 250,000 Bus cases; composed actual-M33 replay; board 6.256 FPS | Regresses versus C; not promoted |
| ARM execution | Native writable-IWRAM arithmetic sequence with event-safe fallback | 7.545 FPS; exact 759,307 native instructions, including 475,803 fast-path instructions | Repeated pairs to distinguish gain from noise |
| Wider Thumb ISA/catalog | Generic non-PC high-register ADD/MOV; 4,096 native entries | Exact local/board counts; 54.91% translated; board 4.972 FPS | Regresses versus C; no further catalog expansion justified |
| Broader ARM blocks | Sample loads, flags and loop operations through original Bus helpers | Local fixtures and strict legacy board endpoints pass; 7.62210 FPS, +1.74734% versus F in one sample | Repeated matched pairs and independent compatibility checks |
| Whole-ROM alternative | Separate static-recompiler toolchain and runtime, outside the existing hot catalog | Zero-interpreter replay, bare-metal runtime and compressed-ROM controls; PSRAM execution/loader probes pass. Native HDMI v1 compiles and passes the startup SRAM-placement audit | First complete board image verified but USB did not reappear; physical recovery and new HDMI/native runtime tests required. No native board FPS |

The CPU agents own separate private source trees. One serial-pinned
board runs experiments sequentially, with a lock and verified V52 restoration
between trials. A local instruction-count reduction is never entered as an FPS
gain. A candidate that fails correctness, allocation or display stability is
retained as evidence and excluded from usable performance claims.

Longer blocks, fewer callbacks and broader native execution address the CPU
architecture. None yet establishes the roughly eightfold overall improvement
needed from today's best matched result. The useful next milestone is a native
CPU candidate that reliably beats the interpreter on the same correct workload.

Private core derivatives, ROM/BIOS bytes, generated native code and raw game
artifacts remain excluded from publication.

The whole-ROM experiment is isolated under private `build/whole-rom-native/`.
It evaluates [GBA-Recomp](https://github.com/JRickey/gba-recomp) at commit
`de4edf59b872d887046d6a3b005e2df551b6d44c`. An emitted native executable is not
automatically a zero-interpreter result: reachable code discovery, copied RAM
code and instruction shells must all be counted. Desktop results will be
reported separately from the Feather benchmark. The completed baseline is a
hybrid, 1.7834× faster than its own desktop interpreter over three pairs. A
follow-up removes all interpreter use in the 5,180-frame replay and preserves
that native baseline's hashes. A separate quieter three-round comparison measures
1.70× its own interpreter; desktop scatter and timing differences remain. No
RP2350 whole-ROM FPS gain has been established.
See the [whole-ROM experiment](WHOLE-ROM-NATIVE-EXPERIMENT.md) for coverage,
state differences, footprint and the compressed-code/PSRAM capacity model.

An independent official mGBA reference is now built and run at commit
`fd9dc5c2cbf234365068855851107e762fc6211b`, using the same real BIOS, ROM and
5,180-frame input policy. Stock/native and mGBA pictures match exactly in 1,416
frames; the existing F direct-framebuffer fixture matches in 632. These are
hash-match counts, not accuracy percentages. F first differs visually at frame
131 in only 28 pixels with a one-unit red-channel difference; sampled non-PC
registers already differ at frame 3 at the same cycle boundary. Final captures
all show demo play, with different camera/game progression. No input or frame
offset was changed to obtain matches. Existing-core correctness checks establish
legacy equivalence, not independent hardware accuracy. Frozen evidence is in
private `build/reference-mgba/`; compatibility remains a separate acceptance
gate for any new runtime or 60 FPS claim.
