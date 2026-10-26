# ROM uploader version history

This history covers the Windows frontend and Python backend independently.
Their canonical versions are in [app-versions.json](app-versions.json).
The emulator's V47 firmware series has its own [development history](PROJECT-HISTORY.md).
An app release does not imply a new emulator optimization or a measured FPS gain.

## Backend 1.0.1 — 20 October 2026

- Added a V52 `prepare flash` acknowledgement before the 1200-baud reset, so a
  planned upload can clear the interrupted-run marker at a drained frame boundary.
- Fixed a real Windows reset race: COM-device disappearance during baud-rate
  reconfiguration can mean the reset succeeded. Continue to probe only the pinned
  USB serial in BOOTSEL, and fail if that exact bootloader cannot be found.
- Retained compatibility with older firmware that lacks the acknowledgement.
  A one-time upgrade from V51 may require an additional software reboot to exit
  its recovery screen. No frontend or JSON protocol version change.
- The original 11 uploader tests plus two reset regressions pass. Physical upload
  and read-back verification succeeded through the new path during the V52 upgrade.
  This does not retroactively validate the Windows frontend's untested upload button.

## Frontend 1.0.0 — 20 October 2026

Status: initial local build, pending complete desktop and hardware acceptance.

- Added a Windows Forms application with a local `.gba` file chooser, COM-port selection, automatic port refresh, build-only and build-and-flash actions.
- Added activity messages, packed-game size, flash usage, cancellation before USB handoff, opening the output folder and retrying an existing build.
- Retained the last ROM and build selection locally. Identified Feather USB devices separately from ESP32 and Bluetooth ports.
- Passed paths as literal arguments to a hidden Python process. No terminal commands are required for normal app use.
- Added the frontend version to executable metadata and the title bar. Passed the actual executable version to the backend for build records.
- Corrected clipped headings observed during the initial desktop inspection, prevented the action row from overlapping when narrowed, and disabled label mnemonics for filenames containing `&`.

Validation: C# compilation; initial UI opened and correctly identified COM9 as the Feather. The user stopped Computer Use with Escape during the subsequent visual check. The revised layout has **not** been visually verified. No upload button was exercised against hardware.

## Backend 1.0.0 — 20 October 2026

Status: build-only integration passed; physical reset/upload pending.

- Added a Python CLI with JSON event protocol **1** for device discovery, progress, failures and completed builds.
- Added ROM metadata, SHA-256 and header-checksum reporting. Accepted input sizes are 192 bytes–32 MiB; fitting a particular game into 8 MiB flash is checked after packing and linking.
- Copied the private emulator into a timestamped job directory, repacked only that copy and built a fresh USB/debug/profile-enabled firmware. Preserved the prepared BIOS when available, otherwise selected the existing MIT replacement.
- Verified packing round-trip, selected-ROM identity, UF2 target/layout/flash bounds and the exact packed bytes at the linked ROM address. Exported the matching native-PC profile map.
- Handled picotool's RP2350 architecture preamble: one physical preamble block may declare a transfer count of two before the separately numbered ARM image.
- Added serial-specific Feather reset and verified `picotool load -v -x` upload. Rechecked COM identity before reset and retained USB serial filtering after BOOTSEL handoff. This path is implemented and mock-tested; physical behavior remains unverified.
- Added cancellable child-process execution, timeouts, confined artifact loading, timestamped output folders, build metadata and an activity log. A quiet-child cancellation test exposed a Windows taskkill failure; a direct termination fallback for the owned child fixes that test.
- Recorded frontend/backend/protocol versions in new build metadata and event logs. Retrying a flash records the uploader versions separately from the versions that built the artifact.

### Build-only integration evidence

The Yoshi's Island test on 20 October 2026 completed at **17:34:59 +01:00** (Europe/Dublin). It exercised the same backend build pipeline used by the app. It was a CLI invocation, before version metadata was added; its original record is preserved without backfilling version fields.

| Measurement | Result |
| --- | ---: |
| Original ROM | 8,388,608 bytes |
| Packed ROM | 3,033,341 bytes |
| Packing round-trip | PASS |
| UF2 file | 6,709,760 bytes |
| Highest occupied flash extent from flash base | 3,354,624 / 8,388,608 bytes |
| Matching final ELF profile ranges | 1,036 |
| Embedded packed-game comparison | PASS |
| Physical upload / gameplay | Not performed |

UF2 SHA-256: `64a8aca846e92da092cb683deddce9b7c459f1367e90d9d5c6d10eb978bbf666`.
The UF2 is larger than its flash payload because it also contains per-block transfer headers and padding.

All **11 uploader tests pass** after the version-tracking and cancellation changes. Tests cover ROM metadata, malformed/wrong-target/out-of-range UF2s, changed device identity, serial-filtered upload commands, cancellation, literal path arguments, subprocess failures, artifact confinement and version reporting. Upload tests use mocks and do not establish board compatibility. The frontend recompiles successfully; executable metadata reports product version `1.0.0` and file version `1.0.0.0`.

## How to maintain this history

1. Update only the changed component in `app-versions.json`: patch for compatible fixes, minor for compatible features, major for breaking behavior. Increment `protocol` for incompatible JSON event changes and update both consumers.
2. Add a dated entry above the corresponding component's previous release. Record the concrete change, reason, validation evidence and unresolved limitations. Use “unreleased” until acceptance is complete; do not invent intermediate versions for work that was never released.
3. Rebuild the executable after changing the frontend version. Its embedded version is the one sent to the backend; editing the JSON alone does not change an existing executable.
4. Preserve each job's `build.json`, `activity.jsonl` and exact PC map alongside the UF2. A CLI build records frontend as `null`. Older artifacts without version fields remain historical records; do not infer their versions retroactively.
5. Record board results separately from host build/test results. Keep emulator V-numbers, app versions, ROM identity and BIOS selection explicit when comparing captures.

This file documents the initial local application implementation. Subsequently,
[firmware V48](PERFORMANCE-v48.md) added an experimental native Thumb translator;
that firmware work does not change frontend/backend version 1.0.0. Full-speed
emulation and the 60 FPS target remain unestablished.
