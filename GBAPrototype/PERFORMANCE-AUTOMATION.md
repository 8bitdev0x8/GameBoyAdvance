# Performance lab 1.0.0

20 October 2026. The user authorized automated tests, USB log capture, firmware
upload and continued performance experiments on the connected Feather.

`tools/perf_lab.py` connects the existing uploader, log decoder and HTML charts.
It is a bounded local workflow. An AI follow-up can inspect evidence and edit
code between runs; the Python tool does not rewrite firmware or invent fixes.

## Capabilities

| Mode | Behavior |
| --- | --- |
| `capture` | Warm up, clear the ring, record an uninterrupted interval, download and decode it, write charts and metrics. No reset or upload. |
| `smoke` | Preserve the current ring, flash one candidate, check boot/progress and capture telemetry; restore the known baseline afterward. If candidate and baseline hashes are identical and healthy, avoid the redundant second upload. |
| `trial` | Preserve the current ring; run baseline/candidate/baseline, rebooting for each timed observation. Leave the baseline running. |
| `build` | Check the prepared ROM/BIOS against a 6,000-frame host replay and core audit, build an isolated ROM-uploader snapshot, validate it, then run `trial`. |

Every run has a Dublin-local timestamped folder under `logs/perf-lab/` with
the request, raw binaries, CSV, HTML charts, summaries and upload activity.
Trials retain UF2 hashes, exact-map fingerprints, target USB identity, failure
and recovery information in `trial.json`. Original inputs/builds are retained.
The OS-local timestamps use the configured computer timezone (Europe/Dublin
on the project computer). UTC offsets are included in JSON records.

## Device and failure handling

- Pin the Feather's USB serial **1CD322C8B24EAF6C**. COM9 is a hint; rediscovery
  follows that serial if Windows assigns another port. Never select the ESP32.
- A write requires a baseline UF2 and its map. Validate UF2 family, flash bounds,
  hash, and the unique embedded profile-map bytes before the first reset.
- Use the uploader's 1200-baud software reset and serial-filtered
  `picotool load -v -x`. Normal uploads should not need physical buttons. V52 adds `prepare flash` /
  `FLASH READY` at a drained frame boundary, clearing the old recovery marker.
  If the host abandons the upload, normal recovery is rearmed after five seconds;
  the watchdog remains active. The first upgrade from older firmware can require
  an additional software reboot to leave its existing recovery screen.
- If boot/capture/health fails, attempt one baseline restoration and retain the
  error. If USB cannot be recovered, stop: hold BOOT, tap RESET, release BOOT.
  Physical disconnects and USB-dead firmware cannot always recover remotely.
- Hold an OS lock across each run. This prevents simultaneous lab runs, including
  scheduled runs. Other serial applications must still be closed.
- Check runtime map, interpreter mode, frame progression, timing partitions,
  discontinuities, IRQ rate, restart changes, heap and retained duration.

The workflow does not prove that the monitor shows a correct picture. Live
frame/IRQ telemetry is a boot/scanout check; reference pixel tests and visual
confirmation remain relevant. A successful fallback does not turn a failed
candidate trial into a successful experiment.

## Usage

This computer has a private, Git-ignored `perf-lab.local.json` with the pinned
Feather, V52 baseline, ROM and BIOS paths. Explicit CLI flags override it.
From any working directory, the common commands are:

```powershell
python <workspace>/GBAPrototype/tools/perf_lab.py capture
python <workspace>/GBAPrototype/tools/perf_lab.py build
```

`build` is the checked build/flash/A-B-A/capture sequence, not just compilation.
Do not run it for an unchanged candidate merely to produce more measurements.


Capture without uploading:

```powershell
python <workspace>/GBAPrototype/tools/perf_lab.py capture --serial 1CD322C8B24EAF6C --port COM9 --warmup 5 --seconds 20 --label sonic-demo
```

Test an already validated candidate. The example deliberately uses V52 as both
candidate and known baseline, so it tests the automation rather than FPS gains:

```powershell
python <workspace>/GBAPrototype/tools/perf_lab.py smoke --serial 1CD322C8B24EAF6C --port COM9 --candidate <workspace>/GBAPrototype/output/GBAFeather-Sonic-v52-USB.uf2 --map <workspace>/GBAPrototype/output/pc-profile-map-v52.json --baseline <workspace>/GBAPrototype/output/GBAFeather-Sonic-v52-USB.uf2 --baseline-map <workspace>/GBAPrototype/output/pc-profile-map-v52.json --warmup 10 --seconds 10
```

For a new candidate, use `trial`, its UF2/map, and the verified V52 baseline.
`build` instead requires `--rom` and `--bios` pointing to the local original
files, plus baseline arguments. The original ROM must match the currently
prepared packed game, and BIOS bytes must match `LocalBios.h`; the loader rejects
testing one game while building another. The existing configured MSVC build
tree `build/gba-host` and Arduino-Pico tools are prerequisites. Paths with spaces
must be quoted in PowerShell. The program passes subprocess arguments as arrays.

Host checks cover the current core audit and full threaded/RGB565 replay, not
all possible guest behavior. Native-emitter changes additionally require the
existing actual-M33 fixture suite. Prebuilt `trial`/`smoke` validates the artifact
and hardware health but does not rerun source correctness tests automatically.

## Performance acceptance

Timed boot windows are **not deterministic input/state replays**. Faster builds
can reach different screens during the same number of wall-clock seconds. PAD
input can also change. The trial reports A1/A2 drift, input/clock differences,
observed FPS and per-frame costs, but always sets `promote: false`.

Therefore scheduled work may prepare and test candidates, but must not replace
the known baseline or claim a general speedup solely from these observations.
Confirm equivalent game work or add a deterministic frame/input replay before
automatically accepting performance gains. Keep controller input unchanged
during timed observations. The scene label is a user/agent annotation, not an
automatic classifier.

## Continuing AI work

On 20 October the user requested a continuously running optimization loop with
parallel agents. The hourly heartbeat was removed and an active 60-FPS goal
started in this chat. Work now proceeds through investigation, isolated candidate
tests, checked builds, serial-specific uploads, captures and analysis. This is
active agent work rather than a Python program that invents patches unattended.
Keep the computer, app, project and Feather available. Tests cannot guarantee
that correct 60-FPS gameplay is achievable on this hardware. Stop hardware retries
if recovery needs physical intervention; preserve evidence and the fallback.

### Fixed-frame experiments (V53 work in progress)

`FrameBenchmark.h` and `tools/frame_benchmark.py` add input indexed by guest-frame
number. Firmware clears the ring at the selected starting frame and freezes guest
execution after the exact measured frame count. The host exports afterward, so
USB speed cannot move either endpoint. `bench status` does not reset frame timers.
Capture validation requires exact consecutive frames, input masks, cycle totals,
no overwritten records, the matching ELF symbol map and normal hardware health.
Comparison additionally requires equal start/end CPU/RAM and native-video hashes
and clocks. These hashes are checksums, not full save states or proof of all CPU
semantics; independent core and pixel tests still apply.

Screenshot inspection corrected the old workload labels: zero input remains at
Select a Language, and the legacy repeated-A script remains at name registration.
The user accepts optimizing these slow screens; they must be reported as menu
benchmarks. Policy 3 uses five-frame presses at 300:A, 600:A, 800:Start, 1000:A and
1200:Start, then releases all buttons. Host screenshots at frames 5000 and 5180
show the moving Sonic city demo; completed frames 5001–5180 form a separate
gameplay workload. Exact on-board results are recorded only after capture.

The default `-FrameBenchmark` build selects this demo policy and window:

```powershell
./GBAPrototype/build.ps1 -Profile -DebugUsb -UsePreparedBios -FrameBenchmark
```

Use an exact UF2/map pair and hold the board lock when coordinating uploads.
Benchmark images freeze after 180 measured frames and override physical pad
input deliberately. Restore a normal build afterward. The initial combined
candidate failed interpreter SRAM allocation; its queued demo trial was cancelled
and V52 restored. The failure is recorded in `PERFORMANCE-v53.md`.

New debug firmware exposes `BOOTFAIL` responses to `bench status` or `boot status`
from explicit allocation-failure and recovered-fault screens. This avoids waiting
for an entire benchmark timeout or requiring a manually transcribed stage.
It also acknowledges planned uploads without dereferencing an absent interpreter.
These handlers require the debug USB build and do not diagnose USB-dead faults.

Real-board acceptance: the initial V51 smoke exposed reset/recovery bugs;
the V52 upgrade bootstrapped successfully after one legacy software reboot;
then a normal V52-to-V52 smoke completed without a recovery detour or buttons.
The successful run is `logs/perf-lab/smoke-2026-10-20_20-43-45-012414/`.

## Version history

- **Frame benchmark 0.1.0, 20 October 2026:** original guest-frame input policies,
  exact interval/frozen endpoints, cycle/input/hash checks, and explicit boot-error
  parsing. This is an independent tool version; the binary log schema is unchanged.
- **1.0.0, 20 October 2026:** initial bounded capture, smoke, A/B/A and checked
  build workflow; USB-serial pinning, exact artifact-map checks, OS lock,
  timestamped reports, one-attempt fallback and explicit no-auto-promotion.
  Mock tests cover failure and restoration behavior. Real-board validation
  results are recorded separately; this tool version is independent of the
  frontend/backend uploader versions and emulator V52.

Tests:

```powershell
python -m unittest discover -s <workspace>/GBAPrototype/tests -p test_perf_lab.py -v
```
