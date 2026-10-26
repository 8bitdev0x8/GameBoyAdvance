# v34d: consistent diagnostic snapshots

The user confirms that v34 runs and FPS improved, with PAD ON and VBskip
20%. Numeric FPS, scene and duration were not supplied. The literal readings
were Run 89, Draw 2, Copy 0, Wait 0 ms/frame, Qfull 16 and Barrier 13 ms/frame,
queue depth four, Bus code fetch 16% and CPU dispatch 17%. The follow-up was
"q full 16bar". These are recorded without silently correcting any numbers.

Total Wait 0 conflicts with split waits 16+13 if from one snapshot. Likewise,
the supplied hotspot ranks and percentages cannot be from one sorted profile
window if #1 is 16% and #2 is 17%. These readings do not justify declaring
renderer waiting solved or calculating a controlled FPS gain.

## Confirmed HUD bug

V34 changed `presentGame` to a five-phase diagnostic rotation, but
`runtimeProgress` still wrote the same row using four phases. That creates
phase collisions and can leave a previous value visible across updates.
The bug is confirmed in source. Whether it caused the user's particular
Wait/Draw readings is not established: total/split counter arithmetic is
unchanged and passes the native regression.

V34d gives `presentGame` ownership of the rotating row during gameplay.
`runtimeProgress` collects HDMI statistics and draws startup text only
before frames begin. The HDMI phase is explicitly redrawn during each
appropriate HUD update, like the other four phases.

The timing row and split-wait row now include the same three-digit snapshot
number, for example:

```
Run 89 Draw 22 Copy 0 Wait 29 ms/f #042
Wait 29 Qfull 16 Barrier 13 ms/f #042
```

This is an illustrative format, not a new board result. The snapshot number
increments once per HUD update and wraps after 999. Values should only be
compared when these numbers match. Individually rounded Qfull+Barrier may
be one millisecond/frame below total Wait. A zero total with splits adding
to 29 cannot be explained by rounding.

## Scope and verification

This is a diagnostic correction to v34, not another execution optimization.
The interpreter, bus, renderer queue, scheduler, ROM/cache, BIOS bytes, UART,
clocks, linker and build flags remain unchanged. The original v34 image
remains the fallback. V34d does not contain the rejected silent-event change.

The build regenerates and checks its PC map against the final ELF. Artifact
validation checks UF2 blocks, address bounds, vectors, stacks, PSRAM, embedded
ROM/BIOS, initialized SRAM and all PC-table entries. Source preservation is
checked against the private frozen v34 snapshot. The existing exact 6000-frame
v34 regression is the engine validation; it is not rerun for this HUD-only
change. Metadata is in `output/diagnostics-v34d-summary.json`.

Flash `output/GBAFeather-Sonic-v34d.uf2` using BOOT + RESET. Banner:
`BOOT v34d - SONIC / HUD SNAPSHOT`. On the same Sonic screen, record numeric
FPS and the two timing rows while the split-wait phase is visible. Also
record IFetch hit/fills when available. This gives a reliable basis for
choosing between remaining renderer barriers and CPU/bus execution work.

The fallback `output/GBAFeather-Sonic-v34.uf2` retains SHA256
`eada8d91a8542564bc5d31e9a795acb1ee61a3a441ee75bb5c651bbb009cc603`.
All firmware, modified core, ROM and original BIOS remain private.
