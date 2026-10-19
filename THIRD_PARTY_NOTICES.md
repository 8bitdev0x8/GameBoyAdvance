# Provenance and publication boundaries

The existing repository's `LICENSE` is GNU GPL version 3. It is retained unchanged. Original project code and documentation published here are provided under that repository licence; third-party components retain their own notices. No claim is made to relicense third-party software, absent sources, game content, or the pre-existing reference documents in `Emulator Development/`.

## Emulator core: excluded

- Upstream: https://github.com/bhoot1234567890/gba-embedded
- Local reference commit: `4ddb79e60f20979b8e438dfa00bf1c71a4a5a289`.
- The upstream README's License section explicitly says no licence to copy, modify or redistribute is granted. GitHub repository metadata also reports no licence.
- The locally modified files under `GBAPrototype/GBAFeather/src/core/` and `GBAPrototype/GBAFeather/src/gba/` are not published. This includes the modified ARM7 interpreter, bus, PPU, APU, IRQ, DMA, timers, cartridge logic and core renderer worker.
- No patch archive, old source ZIP, or compiled emulator image is included as a substitute for those excluded sources. Permission or a licensed replacement is needed for a complete public implementation.

The published integration files refer to that absent API to document the local system. They do not make this a complete rebuildable v24 release.

## tinf: included with notices

- Upstream: https://github.com/jibsen/tinf
- Reference commit: `57ffa1f1d5e3dde19011b2127bd26d01689b694b`.
- zlib licence retained in `GBAPrototype/GBAFeather/src/tinf/LICENSE` and source headers.
- The local decoder uses static decompression-table storage to reduce embedded stack use. That makes this decoder path single-owner and nonreentrant; it is not the unmodified upstream file.

## Replacement BIOS: included with notices

- Cult-of-GBA BIOS: https://github.com/Cult-of-GBA/BIOS
- Embedded header identifies commit `0d9dd3e6283896eab340522d51234799d5c46626` and authors DenSinH, Fleroviux and Sky.
- Obtained through the SkyEmu source tree: https://github.com/skylersaleh/SkyEmu
- MIT attribution/header retained in `ReplacementBios.h` and `ReplacementBios-LICENSE.txt`.
- This is a replacement implementation, not a Nintendo BIOS dump. No SkyEmu emulator source is incorporated into the published port.

## Linker layout: SDK-derived

`GBAPrototype/linker/feather-8mb-stack16k.ld` is based on the Arduino-Pico/Pico SDK linker layout with a project-specific main-stack and scratch-memory arrangement. The Pico SDK BSD 3-clause notice is retained as `GBAPrototype/linker/PICO-SDK-LICENSE.txt`.

## External build dependencies: not vendored

| Dependency | Project link | Use |
| --- | --- | --- |
| HamzaYslmn/esp-ps5 | https://github.com/HamzaYslmn/esp-ps5 | DualSense Bluetooth API, including `latestPacket` |
| Arduino-ESP32 | https://github.com/espressif/arduino-esp32 | ESP32 board support |
| Arduino-Pico | https://github.com/earlephilhower/arduino-pico | RP2350 board support and SDK |
| Adafruit DVI HSTX | https://github.com/adafruit/Adafruit-DVI-HSTX | HSTX video driver |
| Adafruit GFX | https://github.com/adafruit/Adafruit-GFX-Library | Text and diagnostic drawing |

Install these through their normal channels and retain their respective licences when distributing software that incorporates them. Version details in the build guide describe the local tested toolchain, not a claim that they are the latest releases.

## ROMs and generated artifacts: excluded

The private hardware experiment used locally supplied European releases of Super Mario Advance 3: Yoshi's Island and Sonic Advance 3. This repository contains neither game, compressed pages, generated ROM indices, game imagery nor firmware carrying game data. The ROM packer takes a local file; users must supply content they have permission to use.

Local build evidence describes artifact sizes and hashes without distributing the artifacts. All local source archives, compiled outputs, upstream checkouts and personal filesystem paths are excluded. The existing PDFs already present on `main` are preserved unchanged; this publication does not add new reference PDFs or claim ownership of them.
