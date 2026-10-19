# Game Boy Advance on an Adafruit Feather RP2350

An educational hardware experiment: emulate a Game Boy Advance on an **Adafruit Feather RP2350 with 8 MiB PSRAM**, use a **Sony DualSense controller through an ESP32 Bluetooth bridge**, and display the game through the Feather's **HSTX ribbon connector and DVI adapter connected to an HDMI monitor**.

This project grew from a simple question: can I read every controller value on an ESP32? It became a UART bridge, an HDMI input viewer, a memory and video debugging exercise, and finally a running GBA interpreter on the Feather. This repository documents the working pieces, the failed attempts, and the limits of the current result.

**Current hardware result, v28, reported 19 October 2026:** Sonic Advance 3 runs with controller input and HDMI output at approximately **7 emulated FPS**, **PAD ON**, **VBskip 12%**, **Run 113 / Draw 22 / Copy 0 / Wait 14 ms/frame**. The scene and test duration were not specified. PSRAM benchmark values, HDMI IRQ cost and ROM miss/decode figures were not supplied for v28. The 30 FPS minimum and 60 FPS goal remain unmet. A 60 Hz monitor signal does not mean 60 emulated FPS.

Wait is included in Run, and rendering overlaps CPU work; do not add these times. V27 reported 7 FPS, Run 121, Draw 33 and Wait 32 ms/frame. The later report shows lower Draw/Wait values, but the reports do not establish a controlled same-scene FPS speedup.

## Read the development story

- [Architecture](docs/architecture.md): boards, data flow, CPU cores, timing, caches, memory and video.
- [Complete development history](docs/development-history.md): from controller experiments through emulator v28, including failed builds.
- [Debugging guide](docs/debugging.md): how black screens, allocator failures, startup stages and apparent game stalls were investigated.
- [Validation and current state](docs/validation.md): what passed, what was measured on hardware, and what remains unproven.
- [Build and wiring guide](docs/building.md): reproducible support-sketch setup and the prerequisites for the private emulator build.
- [Third-party provenance and publication boundaries](THIRD_PARTY_NOTICES.md).
- [Text evidence](docs/evidence/README.md): build summaries, hardware reports and native test logs through v28.
- [Core audit, v27](docs/core-audit-v27.md): reproduced Thumb transfer bugs, fetch/timer work and remaining accuracy gaps.
- [Performance and profiling, v28](docs/performance-v28.md): video writes, caches, PSRAM/IRQ measurement, acceleration candidates and proposed PC sampling.

## Architecture at a glance

```mermaid
flowchart LR
    Pad[DualSense controller] -->|Bluetooth Classic| ESP[ESP32-D0WD-V3]
    ESP -->|230400 baud UART| Input[RP2350 input parser]
    Input --> CPU[Core 0: ARM7 interpreter and hardware scheduler]
    Flash[Flash: compressed ROM pages] --> Cache[PSRAM backing cache and SRAM hot cache]
    Cache --> CPU
    CPU -->|scanline jobs| PPU[Core 1: GBA scanline rendering]
    PPU --> Row[480-byte SRAM RGB555 scanline]
    Row -->|convert and write directly| Video[SRAM RGB565 display framebuffer]
    Video --> HSTX[DMA and HSTX DVI output]
    HSTX --> Monitor[HDMI monitor]
```

The ESP32 handles Bluetooth and sends controller reports. The RP2350 does the emulation, rendering and video output. No game data is stored on the ESP32, and UART does not carry ROM pages or video. There is no SD card in the tested setup.

## Hardware

| Part | Role / tested configuration |
| --- | --- |
| Adafruit Feather RP2350 HSTX | Emulation and video; 8 MiB flash and 8 MiB PSRAM in this setup |
| ESP32-D0WD-V3, revision 3.1 | Bluetooth Classic controller bridge; 4 MiB flash detected |
| Sony DualSense | Wireless input |
| Matching 22-pin HSTX FPC cable and HSTX-to-DVI adapter | Connect Feather to monitor through an HDMI cable |
| USB power and common ground | Power both boards and establish UART reference |

| ESP32 connection | Feather connection |
| --- | --- |
| GPIO25 / UART TX | RX / GPIO1 |
| GND | GND |

Use 3.3 V UART logic. Initial testing powered each board separately by USB without connecting their power pins. The link is one-way at **230400 baud, 8N1**.

## Controller pipeline

The ESP32 retains the complete 78-byte Bluetooth input report rather than printing a stream of human-readable values. A bounded one-item FreeRTOS queue transfers the newest report from the Bluetooth callback to the UART sender. The sender transmits at up to 100 Hz using COBS framing and CRC-16/CCITT-FALSE.

The Feather validates framing, packet size, version, flags, report ID and CRC. It clears controls after a disconnect report or 250 ms without valid packets. The GBA frontend also latches short button edges across a complete guest frame because a game running at 7 FPS polls much less often than the controller updates.

| DualSense | GBA |
| --- | --- |
| Cross | A |
| Circle | B |
| D-pad or left stick | Directions |
| L1 / R1 | L / R |
| Options | Start |
| Create | Select |

The diagnostic receiver can show sticks, buttons, triggers, touch contacts, raw motion sensors and battery/status fields. The emulator uses only the mapped GBA controls.

## Current emulator state

| Area | State |
| --- | --- |
| Hardware startup | v28 confirmed working by the project author |
| Game tested in current build | Sonic Advance 3, European version; supplied locally, not distributed |
| Earlier game | Super Mario Advance 3: Yoshi's Island, European version |
| Execution | Cycle-scheduled ARM/Thumb interpreter, not a native game port or dynamic recompiler |
| CPU cores | Core 0: interpreter, scheduling, UART, HUD and video IRQ; core 1: scanline rendering and direct RGB565 output |
| Video | 240x160 game image centered in a 320x240 RGB565 buffer; 640x480/60 Hz DVI-compatible output |
| Measured speed | About 7 emulated FPS in the latest user report; full-speed play remains unresolved |
| VBlank accelerator | Active; 12% of guest cycles bypassed in the latest report; this is not utilization |
| ROM storage | Independently compressed pages in Feather flash, decoded on demand into PSRAM |
| Saves | EEPROM/SRAM/Flash emulation; volatile RAM only, no persistence after reset |
| Sound | No audio output; silent APU mode preserves relevant register/FIFO/event behavior |
| Rendering | Direct scanline output; repeated writes avoid joins/invalidation; one display buffer can tear; no intentional frame skipping |
| Compatibility | Experimental; no claim of complete gameplay or original-hardware accuracy |

The latest native test completed **6000 emulated frames** with matching CPU/banked state, exact guest cycles, bus/prefetch/latch state, RAM and every converted RGB565 pixel/display margin between accelerated/compressed/threaded execution and an ordinary synchronous/original-ROM path with conservative video invalidation. It also replaces the display allocation at frame 3000. These paths share the emulator core. Agreement is useful regression evidence, not proof that both are correct against a real GBA.

## Performance history at a glance

All entries are user reports, with unspecified scene/duration. Times are ms/frame; `—` means not supplied. The v25 identification is inferred from the build sequence, not explicitly named in that report.

| Build | FPS | Run | Draw | Copy | Wait | Main work |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| v24 | about 5 | — | 46 | 10 | — | Event-bounded VBlank polling acceleration |
| v25, inferred | about 6 | 144 | 37 | 10 | — | Interpreter helper inlining and SRAM ROM-hit callbacks |
| v26 | 6-7 | — | 49 | 0 | 26 | Core-1 direct video output; full-frame copy removed |
| v27 | about 7 | 121 | 33 | 0 | 32 | Thumb correctness fixes, specialized fetches and cached timer deadlines |
| v28 | about 7 | 113 | 22 | 0 | 14 | Unchanged-write suppression and selective tile-cache invalidation |

V28's scripted native run found 4,141,934 of 4,196,721 video writes unchanged. Decoded tile-row cache misses fell from 8,011,631 to 3,848,078, while almost all scanlines still rendered. Those work counts are not hardware speedup percentages. Read the [full history](docs/development-history.md) for earlier 1-5 FPS builds, black-screen failures and subsequent corrections.

## What is published here

This is a **development record and original support-code repository**, not a complete downloadable emulator release.

The local experiment adapts [`gba-embedded`](https://github.com/bhoot1234567890/gba-embedded) at commit `4ddb79e60f20979b8e438dfa00bf1c71a4a5a289`. Its README explicitly states that it grants no licence to copy, modify or redistribute the code. The modified CPU, bus, PPU and other core sources under `GBAPrototype/GBAFeather/src/core/` and `src/gba/` are therefore **excluded from this publication**. The repository's GPLv3 file does not grant rights to that absent upstream code.

Included: original controller/UART code, HDMI and PSRAM diagnostics, emulator integration/support files, the ROM packer, test harnesses, provenance and development documentation. The controller viewer and standalone hardware tests can be built independently. The GBA frontend and its native tests require the separately held modified core and locally generated ROM assets; **a fresh clone cannot build the v28 emulator as-is**. Upstream alone also lacks this project's private changes.

No commercial ROM, compressed game payload, game screenshot, ROM-containing UF2, compiled emulator binary or archived core source is uploaded. Game titles and numerical test results identify the experiments without distributing game data. See [publication boundaries](THIRD_PARTY_NOTICES.md) before using or extending this repository.

## Repository map

```text
DualSenseReader/       ESP32 Bluetooth-to-UART sender
FeatherReceiver/      Standalone controller-to-HDMI viewer
HDMITest/             Basic HSTX video test
HDMIPSRAMTest/        Early combined video/memory diagnostic
HDMIPSRAMSafeTest/    Staged clock/memory diagnostic
PSRAMDirectTest/      Direct mapped-memory test separated from allocator test
GBAPrototype/
  GBAFeather/         v28 frontend and original support files; core excluded
  tools/             ROM packer and private-reference preparation tools
  tests/             Native test harness; requires excluded core and local assets
  linker/            16 KiB main-stack linker layout and SDK licence
docs/                 Architecture, chronological history, debugging and evidence
Emulator Development/ Existing reference documents retained from main
```

## Where development can go next

The next performance work needs board measurements: per-core PC samples and busy/wait/idle timing, DWT cycle/stall counters, XIP/bus counters and controlled A/B runs. V28 already displays a startup SRAM/PSRAM benchmark and times the original HDMI handler; those measured values have not yet been reported. PC sampling, DWT/bus profiling, a decoded-instruction cache, dynamic recompilation, interpolator rendering and a new DMA scanout design remain proposals. A faster CPU backend would be substantial engineering work. The failed v22 experiment is a reminder that passing desktop tests does not establish embedded startup safety.

Before a complete public emulator release, the core needs redistribution permission or replacement with a suitably licensed implementation. Save persistence, sound, broader game compatibility and stable gameplay also remain open work.

This repository was prepared on the `develop` branch for educational writing. The version names describe actual local build iterations; they are not fabricated historical Git commits. Documentation and code were developed with AI coding assistance and hardware feedback from the project author.
