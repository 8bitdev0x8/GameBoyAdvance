# V49: recover a running emulator with black HDMI

**Board follow-up:** the user confirms that the picture is restored. Both new
captures return to 31.5 kHz HDMI IRQs, with a stable restart counter. The latest
confirmed gameplay capture averages 10.56 FPS. See [board measurements and
next profiling priorities](V49-BOARD-RESULTS.md). References below to pending
hardware checks describe the original packaging state.

The V48 board trial produced a completely black display, with the monitor
still reporting a signal. The retained logs establish that emulation continued;
they do not establish successful visible gameplay or a V48 gameplay speedup.

## Evidence from the two captures

Both binary captures decode with the exact V48 map fingerprint **1963911954**.
All retained frame timing partitions validate. The filenames include
`gameplay`, but the recorded PCs show BIOS startup followed by a short Sonic
interval; the filename is not sufficient to classify a benchmark scene.

| Capture, Dublin local time on 20 October 2026 | 18:41:29.633 | 18:43:25.694 |
| --- | ---: | ---: |
| Retained emulated frames | 66 | 341 |
| Retained frame wall time | 6.61 s | 29.43 s |
| HDMI IRQ/s | 83,041 | 84,043 |
| HDMI handler ms per elapsed second | 390 | 395 |
| HDMI restart counter | 4 | 3 |
| Frames ending with a cartridge ROM PC | 0 | 85 |
| Last guest PC | `00000748` | `080BC26E` |
| Controller connected frames | 0 | 0 |
| Cached free internal heap | 8,360 B | 8,360 B |

The earlier working V47 stream delivers about 31,500 IRQ/s and spends about
147 ms/s in the handler. The new rate is not normal for the same physical
640x480 timing, which generates one IRQ per physical scanline. Native code
does execute in the second capture: its exact sampler marker has 73 hits.
The first capture never reaches cartridge code, so native ROM execution is
not required for the black-screen symptom.

`output/log-analysis-v48-video.json` retains the checksums and decoded figures.
Keep these local captures private. Their mixed startup/menu frame rates must
not be compared with V47's confirmed slow gameplay FPS.

## Recovery change

The installed Adafruit HSTX driver's `reset()` stops HSTX and aborts its DMA
channels, then frees its buffers. It does not reset the HSTX peripheral. The
[RP2350 register documentation](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2350/hardware_regs/include/hardware/regs/hstx_ctrl.h)
specifies that disabling HSTX stops popping the FIFO; stopping it is not a
FIFO-clear operation. Restarting the command expander with residual pixel
words is therefore a plausible command-stream desynchronization path.

V49 resets and releases **only HSTX** after the driver stops its DMA producers
and before `display.begin()` reconstructs the stream. This occurs while the
HSTX clock remains running; the library stops that clock in its early startup
hook, so no extra blocking reset is inserted before the initial begin.
Existing renderer draining, framebuffer
backup/restore and target rebinding remain in place. CPU clocks, UART, ROM,
BIOS selection and native translation are retained.

The existing recovery monitor only detected idle DMA or an interrupt counter
that stopped increasing. The failed stream satisfied neither condition.
V49 also checks line rate: two consecutive windows outside 25–38 kHz trigger
recovery. Checks use windows of at least 500 ms, ignore startup gaps over
2.5 seconds and handle counter/time wrap. Runtime recovery is serviced every
second even if the five-second HUD is not updating. A working stream near
31.5 kHz does not trigger this check.

The FIFO explanation remains a **working diagnosis** until V49 restores the
picture on the board. A log cannot directly prove valid pixels or monitor lock.

## Testing and next board check

The recovery-state tests cover the observed 84 kHz failure, healthy scanout,
transient rejection, startup pauses, reset and wraparound. A 300-frame host
replay with the native semantic model and threaded RGB565 renderer preserves
CPU/frame parity and protected display margins. This shorter replay does not
reach the frame-3000 display replacement. The V48 native CPU/renderer code is
unchanged, so its existing emitted-code tests remain applicable. Logging and
video-status decoder tests pass. The final UF2, assets, stacks and exact PC
map are checked before packaging; measurements are in
`output/performance-v49-summary.json`.

Flash the local `output/GBAFeather-Sonic-v49-USB.uf2`. Confirm the startup
banner says **v49**, then check that the image and HUD remain visible, IRQ/s
returns near 31,500 and the restart counter stops growing. V49 has not been
physically verified during packaging. Keep the complete V47 UF2 as fallback.

Capture with:

```powershell
python <workspace>/GBAPrototype/tools/debug_log.py --port COM9 --output sonic-v49-gameplay --open
```

V49 also accepts `video status` and `video restart` over USB. A PC-side helper
prints frame count, scanout rate/cost, restarts, reported clock frequencies,
HSTX control/FIFO registers, framebuffer address and one diagnostic pixel:

```powershell
python <workspace>/GBAPrototype/tools/video_control.py --port COM9
python <workspace>/GBAPrototype/tools/video_control.py --port COM9 --restart
```

Run one serial tool at a time. A status pixel or active IRQ count does not
prove the monitor can decode the image. Native translation can still be
disabled separately with `block_control.py --port COM9 --engine native --mode off`.

## BIOS residency correction

Both V48 captures have `bios_sram=0`; the V47 gameplay capture also has this
flag clear. These builds are using the **flash BIOS view**, not a successful
SRAM copy. Preserving linked heap headroom did not prove that the conditional
16 KiB BIOS allocation occurred. V49 leaves the existing 8 KiB heap reserve
and conditional-copy behavior unchanged. Do not attribute these results to
a BIOS SRAM copy.
