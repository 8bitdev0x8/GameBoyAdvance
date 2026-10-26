# Small FPS status

25 October 2026, Dublin. Values below are retained board observations unless
otherwise stated. A version number is not proof of an improvement.

| Recent build | Relevant change | Retained slow-window FPS | Difference |
| --- | --- | ---: | --- |
| V50 | Interpreter default after native-mode regression | 7.4613 | Reference |
| V51 | Compile-time specialization of normal interpreter slice | 8.8637 | +1.4024 FPS / +18.80% observed versus this V50 window |
| V52 | Planned-upload handshake and automation support; same core as V51 | No matched slow-gameplay result yet | No FPS gain claimed |
| V53 repaired baseline | Fixed automatic Sonic demo; early SRAM reservation and PSRAM PCM storage | 6.800 | New exact-frame baseline; earlier scene windows are not comparable |
| V53 memory/render candidate | Guarded RAM transfers and zero-strength brightness compositor | 7.039 | +0.239 FPS / +3.51% in the first matched demo pair |
| V53 IWRAM fetch candidate | Direct, uncached opcode reads from actual internal guest RAM | 7.265 | +3.21% versus memory/render; +6.83% versus repaired baseline, first matched trials |
| V53 compact HSTX DMA candidate | Frame-interrupt DMA scanout experiment | Rejected | User observed flicker/corruption; no valid gameplay FPS gain claimed |
| V53 pinned ROM line | Remove the redundant 256-byte stable-window copy | 7.101 | +0.88% versus memory/render in one matched trial; timing noise not yet quantified |
| V53 Flash AOT128 baseline | Execute 128 pretranslated ROM Thumb blocks | 5.473 | −24.67% versus IWRAM interpreter; correctness passes, do not promote |
| V53 resolved-color/fused renderer | Smaller resolved-color tile cache and eligible fused composition | 7.491 | +3.12% versus IWRAM interpreter; +10.16% versus repaired baseline, first matched trials |
| V53 native-only SRAM CPU loop | Specialized native dispatch, negative lookup bitmap, unused-cache reclamation | 5.892 | +7.66% versus original AOT, still −18.90% versus IWRAM interpreter; do not promote |
| V53 matched Flash CPU loop control | Identical specialized native loop placed in Flash | 5.497 | Same loop is about 7.18% faster from SRAM in this matched pair; both trail interpreter C |
| V53 F + guarded native ARM sequence | Independent arithmetic kernel in the normal interpreter | 7.545 | +0.72% versus F in one matched trial; noise unmeasured, no reliable gain claimed |
| V53 F + typed native ARM extension | Sample loads, flags and loop work through original Bus helpers | 7.622 | +1.74734% versus F's 7.49120; one sample, not statistical proof |
| V53 early native admission, attempt 02 | Reject definite catalog misses before the Flash admission routine | 6.293 | +6.82% versus prior SRAM native loop; still −13.37% versus interpreter C |
| V53 bounded native fetch pairs | Batch eligible ROM fetch bookkeeping with early admission | 6.256 | −13.88% versus interpreter C; not promoted |
| V53 4,096-entry native catalog | High-register support and 54.91% native guest-instruction coverage | 4.972 | −31.55% versus interpreter C; more hybrid coverage is slower, not promoted |
| Native HDMI v1 | Separate all-native runtime and core1 scanout IRQ | 3.998 | Own-runtime endpoint checks pass; black picture and abnormal scanout, excluded from usable gains |
| Native HDMI v2 | Core0 scanout IRQ; colour bars and pixel/DMA diagnostics | No completed capture | Picture initially visible, then black during ROM scanning; last saved progress frame 2,000 |
| Native HDMI v3, 25 October | DVI timing/header constants moved into SRAM | No accepted FPS window | Scanout rises from 31,472 to 84,066 IRQ/s during ROM scanning; stopped early and V52 restored/verified |

V50 and V51 were not deterministic matched replays. The percentage is the
arithmetic difference between two scene windows, not a proven code-only gain.
The V51 captures range from 8.86 to 14.40 FPS because workloads differ. V52's
16.54-FPS early boot/demo smoke capture validates the workflow; it must not
be compared to V51's slow gameplay as a speedup.

The separate preserved-V50/V51 local test shows 3.86% less CPU-only desktop
elapsed time and 4.18% less threaded desktop elapsed time. Actual M33 synthetic
loops execute 4.15–4.49% fewer host instructions. These are not board FPS.

One stronger same-session observation is V49's native A/B/A trial: weighted
interpreter 7.1945 FPS versus native 5.3141 FPS. Interpreter selection is
35.39% faster relative to native in that trial; native is 26.14% slower relative
to interpreter. Zero input and A1/A2 drift limit generalization. That is a mode
comparison within V49, not a measured firmware-to-firmware V49/V50 gain.

The [V1–V49 retrospective](PERFORMANCE-RETROSPECTIVE.md) has the earlier
version-by-version record, including failed boots and missing measurements.
Early Yoshi and later Sonic readings cannot support a defensible overall
percentage speedup.

V53 experiments add frozen guest-frame windows and deterministic input, including
separate language-menu and confirmed demo-level workloads. Memory multiple-transfer
and zero-strength-brightness candidates passed independent fixtures and a combined
6,000-frame CPU/pixel replay. The fixed language baseline is 14.97 FPS; the
combined candidate then failed to allocate interpreter SRAM during boot. V52
was restored successfully. The SRAM repair subsequently boots: its fixed demo
baseline completes frames 5001–5180 at 6.800 FPS, with 17,400 bytes free heap,
no lost records and healthy telemetry. Its paired memory/render image completes
the same interval at 7.039 FPS: a 3.51% increase in this first pair, with matching
CPU/RAM/video endpoint hashes and guest cycles. Draw falls 36.906 to 32.215 ms;
Wait falls 15.473 to 13.024 ms. Repeated pairs are needed to quantify noise.
The IWRAM-fetch image then completes the same interval at 7.265 FPS with exact
matching endpoints, a further 3.21% observed increase. Its game-thread elapsed
time outside renderer waiting is 117.456 ms/frame, versus 121.817 for the preceding
candidate. This still includes interrupt and memory-stall time.
The compact DMA-scanout trial was stopped after the user observed flicker or
corruption. Its timing data cannot establish a usable display performance gain.
ROM-line pinning and immutable Flash translation remain separate experiments.
Earlier desktop scripted timings concern menus/name
registration, not gameplay. The [automation guide](PERFORMANCE-AUTOMATION.md)
describes the continuous goal and stricter comparison checks.

The typed ARM trial completes the same fixed demo at **7.622097 FPS** with all
strict legacy endpoints and guest cycles passing. Run is 124.57575 ms/frame,
Draw 24.61482, Wait 9.532294, and Run outside Wait 115.043456. Draw overlaps the
game thread; these values must not be added as independent costs. The observed
increase over F is small and needs repeated matched trials. Evidence:
`logs/perf-lab/v53-arm-typed-demo-sequence-01/`.

The first complete whole-ROM native board image uploaded and verified at
00:58:22 Dublin on 21 October, but the Feather did not reappear over USB and no
startup record was captured. The attempt was stopped; one V52 fallback attempt
also failed because neither runtime USB nor serial-pinned BOOTSEL was available.
Manual BOOT/RESET recovery is required before further hardware work. Evidence is
under `logs/perf-lab/native-runtime-board-01/`. HDMI was intentionally absent in
that headless image, so a blank display alone was not a failure indication.
Native HDMI v1 now compiles with startup diagnostics and game display. Its
identified QMI startup dependencies pass an independent SRAM-placement audit;
USB, HDMI stability and native gameplay FPS still require a recovered board.
Native HDMI v1 now completes its own runtime's physical endpoint/counter checks
at **3.997586 FPS**, but the user reports a black picture with signal retained.
Its conservative IRQ-rate lower bound exceeds 80,963/s versus roughly 31,500/s
expected. It is excluded from usable HDMI performance gains. V52 is restored
and verified. V2 later showed startup bars but went black after ROM scanning;
its capture is incomplete. The 25 October V3 SRAM-data control reproduced the
abnormal scanout rate and stopped before the measured window. Its runner
restored V52 and verified runtime map fingerprint 2447646605. No V2/V3 FPS gain
is claimed. See
[the startup investigation](NATIVE-STARTUP-DEBUGGING.md).

Independent compatibility is also unresolved. Pinned official mGBA now runs the
same real BIOS/ROM/input schedule through all 5,180 frames. Its exact picture
hashes match stock/native in 1,416 frames and the F direct-framebuffer fixture
in 632, without shifting inputs or frame indices. These counts are not accuracy
percentages: F's first picture difference at frame 131 is only 28 pixels with a
one-unit red-channel discrepancy. Later demo progression differs visibly, and
sampled non-PC registers diverge by frame 3. See the
[whole-ROM record](WHOLE-ROM-NATIVE-EXPERIMENT.md) and private
`build/reference-mgba/` for the frozen reference, conversion and boundary scope.


## 26 October: Casper direct-Flash callback fix

A fresh same-ROM/same-input/same-frame board A/B measured10.49835 to12.04845 FPS (+14.77%) by supplying the missing immutable instruction-line callback. CPU/RAM/video endpoint hashes and cycles match. Static SRAM unchanged; no CPU/bus/timing implementation changes. See PERFORMANCE-2026-10-26.md for evidence and scope. This is a scripted Casper window, not a full-game average; native headless figures elsewhere are separate experiments.


Current deployment at the user’s request: verified Sonic V52 USB at240MHz (HSTX126MHz), fingerprint2447646605; no fresh Sonic FPS result. A subsequent300MHz attempt used an invalid1.6 fractional divider and failed to enumerate USB. Its firmware was quarantined, the runner disabled, and BOOTSEL recovery to Casper240MHz was verified before the Sonic restoration. There is no valid300MHz FPS result and this failure does not establish a silicon speed limit. Event-bounded executor rewrite remains pending.
