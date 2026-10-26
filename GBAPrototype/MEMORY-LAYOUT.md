# Memory in this Feather GBA emulator

The RP2350's internal RAM and OTP are inside the microcontroller. This
Feather adds separate 8 MiB flash and 8 MiB PSRAM chips. Both cores share
the memory; 520 KiB is the chip's total SRAM, not 520 KiB per core.

| Memory | Capacity | Survives power loss? | Role |
| --- | ---: | --- | --- |
| Internal SRAM | 520 KiB | No | Fast working data, stacks and selected hot executable functions |
| External QSPI PSRAM | 8 MiB | No | Large, slower working buffers |
| External QSPI flash | 8 MiB | Yes | Firmware, compressed cartridge, BIOS and constant tables |
| Internal OTP | 8 KiB | Yes | Permanent configuration/provisioning; not emulator working RAM |

Board capacities are documented by [Adafruit](https://www.adafruit.com/product/6130).
Raspberry Pi describes [SRAM and OTP/security](https://www.raspberrypi.com/products/rp2350/)
and [one-time programmable memory](https://github.com/raspberrypi/documentation/blob/master/documentation/asciidoc/microcontrollers/microcontroller-chips/rp2350.adoc).
Here KiB means 1,024 bytes and MiB means 1,048,576 bytes.

## Internal SRAM: what is actually fast

| Item | Allocation or behavior |
| --- | --- |
| Emulated GBA IWRAM | 32 KiB |
| Emulated GBA VRAM | 96 KiB |
| Palette + sprite attributes (OAM) | 1 KiB each |
| Small bus timing storage | 64 bytes |
| Hot ROM cache | 32 KiB, plus a separate 256-byte instruction window |
| Live HDMI framebuffer | 150 KiB, allocated by the display driver's ordinary `malloc()` in internal SRAM |
| Emulator object | CPU/banked registers, bus/device state, PPU tile/sprite caches and row workspace; 83,560 bytes in V43 |
| V44 decoded Thumb block cache | 32 entries x 104 bytes = 3,328 bytes, plus counters/control; inside the enlarged emulator object |
| Render worker | 4 queued scanline jobs, register snapshots, a 240-pixel row and counters; 1,032 bytes in V43 |
| Stacks | 16 KiB reserved for core 0; 8 KiB plus 64-byte canary for core 1 |
| Hot native functions | Interpreter, bus readers, renderer, IRQ handling and other explicitly SRAM-placed code; initial bytes also occupy flash |
| Runtime | Heap metadata, USB/UART buffers, controller state, profiler and SDK globals |
| Optional BIOS copy | 16 KiB only if startup can retain the configured 8 KiB heap reserve; otherwise the BIOS is read from flash |

The emulator/worker sizes include several listed substructures; **do not sum
all rows as independent allocations**. V43's Arduino report is 239,724 bytes
of static SRAM, including SRAM-resident code and static buffers. It excludes
the later dynamically allocated emulator object and other heap allocations.
The board V42 logs report about 20 KiB free heap at their sampling times.
V44's exact final sizes are recorded in `output/performance-v44-summary.json`.

The custom linker uses the first 496 KiB of the main 512 KiB region for
code/data/heap and reserves its top 16 KiB for the main stack. Two additional
4 KiB scratch regions complete the chip's 520 KiB SRAM map. A linker region
being available does not mean its contents are dynamically free heap.

## External PSRAM: project arena

The project reserves a 7.5 MiB (7,680 KiB) fixed-lifetime arena. It avoids
individual frees and allocator metadata stored in external RAM. A reset
reclaims the whole arena. These are the V43/V44 allocations with USB logs:

| Allocation | KiB | Purpose |
| --- | ---: | --- |
| Decoded cartridge blocks | 7,040 | 6.875 MiB second-chance ROM cache; decoded 16 KiB blocks are evicted/reloaded |
| Emulated GBA EWRAM | 256 | The guest game's main external working RAM |
| Cartridge save capacity | 128 | Emulated save-chip storage; currently volatile |
| GBA RGB555 framebuffer | 75 | 240 x 160 x 2 bytes; retained allocation even with direct HDMI output |
| HDMI recovery backup | 150 | 320 x 240 x 2 bytes; preserves pixels during display recovery |
| Binary debug log | 24 | Ring containing frame timings and periodic CPU profiles |
| **Allocated total** | **7,673** | **7,857,152 bytes** |
| **Arena spare** | **7** | **7,168 bytes** |

The other 512 KiB of the 8 MiB PSRAM is left outside the arena for SDK
allocations. This is a reservation split, not a measured free-space value.
**Correction from the allocator audit:** the live 320 x 240 RGB565 display
buffer (150 KiB) and the driver's dynamically allocated scanline buffers use
ordinary `malloc()`, which this Arduino core routes to internal SRAM.
Only the explicit 150 KiB recovery backup above is in the PSRAM arena.
Earlier descriptions placing the live display buffer in PSRAM were incorrect.

The 75 KiB GBA framebuffer is a possible allocation to remove in a dedicated
direct-output-only design. That would require changing framebuffer ownership,
fallback/reset behavior and tests. The current renderer still requires its
allocation even when an external output row is supplied. Reclaiming it would
save capacity, not automatically improve pixel computation time, and 75 KiB
alone would not fit another complete 98 KiB video-memory image.

## External QSPI flash: what persists

Flash contains the emulator firmware, the selected 16 KiB BIOS, the packed
Sonic ROM, its block index/CRCs and other constant tables including the exact
PC profiler map. The current 16 MiB original cartridge compresses to
4,753,067 bytes (about 4.53 MiB). It is not transferred from the ESP32.

Most native code executes through the flash/XIP mapping. Selected hot
functions are copied into internal SRAM at startup. Similarly, decompressed
ROM blocks live in PSRAM only while running; flash retains their compressed
source across resets.

The Arduino flash layout here provides no general game-save filesystem.
Flash could be used for persistent saves with a separate design for space,
erase/write cycles and safe coordination with execution/display. The current
build does not implement that. OTP is not an alternative save filesystem.

The UF2 download file can exceed 8 MiB because it contains transport headers,
padding and block packaging. Its disk size is not the actual flash payload.
The package validator checks the embedded flash addresses and exact bytes.

## What does 8 KiB OTP mean?

OTP is **one-time programmable**, nonvolatile storage. Programming changes
are permanent rather than ordinary erase-and-rewrite operations. RP2350 uses
it for chip information and optional configuration/security provisioning,
including boot-related settings and protected key material. The project does
not provision or use OTP for ROM, RAM, logging or saves. It is separate from
the 520 KiB working SRAM and cannot be treated as another 8 KiB of heap.

## Physical addresses versus guest addresses

In this build, RP2350 flash is mapped around `0x10000000`, PSRAM around
`0x11000000`, and SRAM around `0x20000000`. A game's GBA addresses are
different: for example GBA EWRAM `0x02000000` and VRAM `0x06000000` are routed
by the emulator bus to their respective physical backing allocations.

QSPI means a quad-data-line serial interface. It describes the connection,
not whether the attached device is flash or RAM. Internal SRAM avoids those
external serial transactions. Cached sequential reads can behave very
differently from dependent random reads, so multiplying one PSRAM latency
test by every memory access is not a valid model of the whole emulator.
