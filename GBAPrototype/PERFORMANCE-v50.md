# V50: use the faster measured interpreter default

The V49 board A/B/A trial shows that the current native translator regresses
the measured workload. V50 changes the startup default to **interpreter**,
while retaining USB native-mode opt-in, HDMI recovery, the selected Sonic ROM,
BIOS, clocks, input path and renderer. This removes a measured regression;
it is not a new execution backend or a 60-FPS result.

## Board evidence

Trial directory: `logs/native-ab-2026-10-20_19-40-17-466793`.
Each phase follows five seconds of warmup and approximately thirty seconds
of measurement. The raw captures all match V49 map fingerprint **3498996950**,
pass CRC/frame-partition checks and contain the expected native-mode flag.
No records were overwritten. The original manifest is preserved unchanged.

| Phase | Frames | Retained time | Weighted FPS | Run ms/f | Draw ms/f | Wait ms/f |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A1, interpreter | 227 | 30.207 s | **7.515** | 130.85 | 36.54 | 16.17 |
| B, native | 161 | 30.297 s | **5.314** | 184.91 | 36.25 | 10.37 |
| A2, interpreter | 208 | 30.255 s | **6.875** | 143.18 | 36.76 | 15.74 |

Combining the two interpreter observations by frames/elapsed time gives
**7.195 FPS**, compared with **5.314 FPS** in native mode. Native is 26.14%
lower; selecting the interpreter is 35.39% higher relative to that native
observation. These percentages describe this sequential trial, not all games
or a measured V50-firmware speedup. Interpreter A1/A2 drift is **8.89%**.
Native loses to both individual interpreter observations.

The p95 frame times are 149.44, 212.83 and 156.27 ms respectively. The native
regression therefore also appears in slow-frame timing. Draw remains near
36 ms/frame, with comparable background/composition work in all phases.

All phases have zero controller input and PAD OFF throughout. The original
benchmark warning treated any disconnection as inconclusive. Uniform zero
input is nevertheless useful for this CPU-throughput comparison; it says
nothing about controller responsiveness. The tool now records this uniform
condition as a note, while retaining warnings for partial/mixed connection
states. It still labels a sequential trial as an observation rather than a
deterministic state/input replay.

HDMI remains at about **31,500 IRQ/s**, with 146.9 ms/s of handler cost and
reset counter **10** unchanged across all three phases. ROM misses/decode
are zero in the retained windows, and cached internal free heap is **8,336 B**.
These measurements do not support blaming this regression on decompression,
a new display failure, or a changing input state.

## What the translator counters reveal

Over a separate 30.078-second status-counter interval, native mode reports:

| Counter / derived rate | Value |
| --- | ---: |
| Eligible lookup attempts | 4,619,361 |
| Compilations | 61,183 |
| Native calls | 389,923 |
| Native guest instructions | 1,310,817 |
| Typed memory operations | 226,521 |
| Pending fetched-instruction exits | 10,372 |
| Opcode mismatches | 0 |
| Compilations per second | **2,034** |
| Native guest instructions per call | **3.36** |
| Native calls per compilation | **6.37** |

Only 8.44% of eligible lookup attempts end in a native call. This is not a
guest-instruction coverage percentage or a measured cache-hit ratio: attempts
also include admission rejection, and new compilations execute immediately.
Likewise, a compilation count does not reveal how many entry PCs were unique
or how often each was recompiled. Explicit eviction/recompile/exit-reason
counters remain future work.

The executable pool is only 2,304 bytes, and calls frequently execute short
prefixes. Native mode still pays lookup/admission, context setup, timing/fetch
gates and typed bus helpers. The board measurements now show that the saved
decode work does not compensate for those costs in this workload. Earlier
warm synthetic loops were insufficient evidence to enable it by default.

Run outside explicit renderer wait rises from a weighted interpreter
**120.78 ms/frame** to **174.53 ms/frame** in native mode. The worker's lower
queue-full waiting during native mode is a consequence of the slower producer,
not proof of a faster whole emulator. Run and Draw overlap and are not summed.

Full decoded evidence: `output/native-ab-v49-analysis.json`.

## Applied change and firmware

The agent sent `native off` to the connected Feather and received an
`enabled: false` acknowledgement. This applies the measured faster mode to
the current V49 session without flashing. The acknowledgement is saved as
`applied-native-off.json` inside the trial folder. Resetting V49 would return
to its old native-on default.

V50 uses `set_native_execution(false)` during setup. Its banner identifies
**v50 / INTERPRETER DEFAULT**, and normal HUD operation should show **J0**.
USB `native on`, `native off` and `native status` remain available. The emulator
core files themselves are byte-identical to the V49 private snapshot; the
firmware behavior change is the startup selection and identifying text.

Local firmware: `output/GBAFeather-Sonic-v50-USB.uf2`.
The V50 UF2 is not flashed by the agent. Its selected execution path was
measured on V49; that is distinct from a completed V50 boot test.

## Validation and limits

The interpreter-default 6,000-frame host replay compares CPU/banked state,
cycles, bus/prefetch/latches, RAM and displayed RGB565 pixels against the
conservative original-ROM path. It includes the threaded renderer and
frame-3000 display-target replacement. The twenty checkpoints match the
retained reference. The replay is a correctness check, not a board benchmark.

Nine benchmark/control tests pass, including uniform disconnection handling;
the binary decoder's 14 tests pass with one optional-fixture skip. Packaging
checks the final UF2, exact profile map, ROM/BIOS bytes, initialized SRAM,
16 KiB main stack, unchanged core sources, SRAM budget and preserved V49/V47
fallback hashes. Details are retained in `output/performance-v50-summary.json`.

The remaining workload still runs around seven FPS in the interpreter trial,
well below the 60-FPS target. Before another native default, redesign and
measure the fast entry/admission path, code retention and useful work per call
against this interpreter baseline. A code-cache change alone is not assumed
to solve helper/exit overhead or all remaining CPU work. Keep that experiment
separate from display, clock and renderer changes.
