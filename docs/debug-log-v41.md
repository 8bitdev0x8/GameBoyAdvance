# v41: compact binary frame logging over USB

V41 adds measurement storage and export to the running v40 emulator. There
is no renderer/CPU optimization in this version. It retains Sonic, the same
BIOS policy, 240 MHz CPU, 30 MHz PSRAM, HDMI and ESP32 controller UART.
The startup banner is `BOOT v41 - SONIC / USB LOGGER`.

## Use it

1. Flash `output/GBAFeather-Sonic-v41-USB.uf2` by the usual BOOTSEL method.
   Keep the unchanged `output/GBAFeather-Sonic-v40.uf2` as the manual fallback.
2. Let the game reach the scene being investigated and play for a while.
3. Keep the Feather connected with a USB data cable. List serial ports:

   ```powershell
   python .\GBAPrototype\tools\debug_log.py --list
   ```

4. Close any Serial Monitor holding the **Feather's** port. Download using
   that port, substituting its COM number below. The ESP32's CH340 port is a
   separate device and does not export this log.

   ```powershell
   python .\GBAPrototype\tools\debug_log.py --port COMx --output sonic-test.bin --open
   ```

Captures now default to `GBAPrototype/logs`, independent of the shell's
working directory. The prefix receives an OS-local timestamp including
milliseconds, such as `sonic-test-YYYY-MM-DD_HH-MM-SS-mmm.bin`. An existing
base filename is never overwritten or treated as a capture error. `--log-dir`
selects a different default folder; an explicit directory in `--output`
takes precedence. All companion files use the same timestamped stem.

The utility saves the original binary and creates `-frames.csv`,
`-profiles.csv`, `-summary.json` and an offline interactive `.html` report.
`--open` opens the report in the default browser. It includes frame timing,
stacked renderer stages, FPS and CPU hotspot charts with linked per-frame
inspection. It uses no external libraries or network requests. The summary includes
weighted FPS, worst/median frame time, average stage times, overwritten
records and invalid timing partitions. Raw integer fields remain in the
CSVs alongside derived milliseconds/percentages. Download requires pyserial
(already installed in this workspace); offline decoding uses only Python's
standard library.

To decode a saved file later:

```powershell
python .\GBAPrototype\tools\debug_log.py --input sonic-test.bin --map .\GBAPrototype\output\pc-profile-map-v41.json
```

The matching map is discovered automatically in this project's output
directory when its fingerprint matches. Without a map, frame data still
decodes and CPU functions appear as numeric IDs. An explicitly supplied
wrong map is rejected. Keep the exact map with the binary log.

The USB serial commands are newline-terminated `dump`, `clear` and `cancel`.
`dump` produces binary bytes, so use the download utility rather than a text
terminal to capture it. There is no mass-storage drive or dashboard.

## What is retained

During normal gameplay, **every completed emulated frame** contributes a
record, rather than only whichever five-second HUD page happens to be on
screen. Frame records include:

* Frame number, uptime and wall time between completed frames.
* Run, Draw, Copy, total Wait, Qfull and Barrier elapsed microseconds.
* Prep, BG, OBJ, Mix, RGB and Other renderer elapsed microseconds, worker
  total, consumed worker jobs and dirty jobs.
* ROM-block misses/decompression time and instruction-window hits/fills.
* Executed guest-cycle progress and VBlank polling cycles skipped.
* HDMI IRQ count/time, HDMI restart count and guest PC.
* Controller connected state, GBA input buttons, guest key-register read
  count and selected BIOS storage mode.
* The latest cached free-heap reading, refreshed every five seconds.

CPU-sampling snapshots are separate records at the existing roughly
five-second update cadence. They include sample count/window duration,
invalid/unmapped/IRQ samples, sampler handler cost, free heap and the top
three function IDs/percentages. IDs resolve against the exact final ELF
map. The sampling window is independent of individual frame intervals.

The file header also retains CPU/PSRAM frequencies, all three dependent-read
memory benchmark results, the map fingerprint and the number of overwritten
records. No ROM bytes, BIOS bytes or full bus/memory trace are recorded.

Frame timing validation checks that the six drawing stages sum to worker
time, worker time equals Draw, and Qfull + Barrier equals Wait. The host
rechecks those identities. This removes transcription/rounding ambiguity.
Run includes waiting; core-1 Draw overlaps core-0 Run. Do not add all these
numbers together as independent frame costs. Wall time also includes work
outside Run, such as input service, diagnostics and frame logging.

## Memory and capture lifetime

The ring uses **24 KiB of the existing fixed PSRAM arena**, leaving 7 KiB
of that arena unused. It does not take space from the ROM cache or use the
previously problematic PSRAM heap. Small encoder/transport buffers and
indices live in internal SRAM; there is no per-frame dynamic allocation.

Records are sparse binary integers: zero fields are omitted using a bitmap,
and remaining values use unsigned variable-length integers. Function names
are represented by IDs rather than copied into every record. Values retain
their integer precision; there is no floating-point or text formatting in
the recorder. The native Sonic regression averaged approximately 50 bytes
per record, but physical-board records contain additional IRQ/Run fields
and their sizes will differ. Retention is measured in bytes, not a promised
number of seconds. Expect a rolling recent history, not unlimited storage.

When the ring fills, it evicts **whole oldest records** and counts the
evictions. A PC profile can be retained without all its surrounding frame
records, or vice versa; use the frame numbers/window durations accordingly.
All logs are volatile: reset or power loss clears them. Export before reset.
This is runtime performance logging, not a retained crash dump.

## Export behavior and measurement limits

The recorder freezes while exporting, and core 0 stops advancing the guest.
Core 1 is drained first; HDMI, controller polling and watchdog service
continue. The export uses available USB TX space, capped at 64 bytes per
write, and handles partial writes. Completion, cancellation or disconnect
resumes emulation. An export with no transmit progress for ten seconds is
cancelled automatically.

Export starts at a completed-frame boundary. Timing baselines reset at that
same guest boundary when it ends, so paused wall time is excluded and no
completed game frame is intentionally omitted. The unfinished five-second
HUD averaging window is restarted. USB command polling occurs once per
frame during normal gameplay, rather than on every CPU slice.
The first CPU-sampling snapshot after export is flagged because its
window may contain export work. Clear/export operations do not alter the
guest CPU/PPU/DMA timing model. Held/live inputs may change during the pause,
as they would in any interactive pause.

The current firmware previously used `No USB`. V41 enables the installed
Pico SDK CDC stack, whose USB interrupts and service work can affect FPS.
Binary encoding and PSRAM writes also cost time. No zero-overhead claim or
FPS gain is made. BIOS copy/fallback may change if USB reduces available
heap; the log records the selected mode. Use this build to locate expensive
frames, then verify optimizations in a build with the same instrumentation
or with instrumentation disabled under controlled conditions.

## Binary format v1

All fixed scalars are little endian. A 64-byte header contains:

| Offset | Contents |
| --- | --- |
| 0 | Eight bytes `GBALOG41` |
| 8 | Four u16 values: schema 1, header size 64, frame fields 32, profile fields 16 |
| 16 | Eleven u32 values: payload bytes, records, overwritten records, CPU Hz, PSRAM Hz, map fingerprint, S32/P32/P256 dependent ns, flags, export uptime ms |
| 60 | CRC32/IEEE of bytes 0–59 |

Each chronological record is `u16 total size`, `u8 type` (1 frame, 2 CPU
profile), `u32 nonzero-field bitmap`, nonzero fields in ascending bitmap-bit
order as unsigned LEB128 u32, then CRC16/CCITT-FALSE over all preceding
record bytes including its size. Maximum record size is 169 bytes. Field
orders are defined in `DebugLog.h` and `tools/debug_log.py`. Records end at
the header's declared payload length, followed by eight bytes `GBAEND41`.

The map fingerprint is FNV-1a/u32 over the used final `GbaPcRange` entries
(`u32 begin`, `u32 end`, 24-byte zero-padded label). It is an identity check,
not a cryptographic authenticity mechanism. CRCs detect accidental data
corruption, and the decoder rejects truncation, invalid lengths, malformed
varints and record-count mismatches.

## Validation

* C++ tests cover standard CRC vectors, wrapping counters, absolute fields,
  maximum/sparse varints, chronological ring wrap/eviction, empty export,
  zero/partial writes, cancellation and clear/freeze behavior.
* Eight independent Python tests cover the C++ fixture, integer boundaries,
  bit 31, corrupt/truncated packets, invalid varints, exact map IDs/fingerprint,
  weighted FPS and simulated USB partial reads.
* A 6,000-frame Sonic regression records binary frame/profile events and
  exports through partial writes. Every timing partition is checked; CPU,
  cycles, bus state, RAM and every RGB565 pixel still match the reference.
  All 20 checkpoints match v40. The PC decoder checks every retained record.
* The USB build has more functions, so the fixed profiling table grows from
  1024 to 1152 entries. Its generated map is checked against the final ELF.

Physical USB enumeration/export is now confirmed by a 325-frame capture:
25.85 seconds, weighted 12.57 FPS, worst frame 209.1 ms, no frame gaps or
invalid timing partitions. Average Run 75.29, Draw 38.40 and Wait 25.45
ms/frame; Mix 20.38 ms is about 53% of Draw. BIOS uses flash view in this
capture. USB/logging overhead and a controlled optimization comparison
remain unmeasured. V40's reported range is
4–20 FPS across different game frames, not a sustained controlled average.
Its Mix 20.3 reading belongs to a different interval from Draw 18.3 at #029;
the reported W42.6/Q42.4/B0.8 row also fails arithmetic consistency and is
retained literally. Binary logging is intended to resolve those uncertainties.

All modified emulator core, ROM, BIOS and firmware artifacts remain private.
