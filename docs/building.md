# Wiring, tools and build boundaries

The independently buildable parts of this repository are the ESP32 bridge, Feather controller viewer and standalone HDMI/PSRAM diagnostics. The GBA frontend and emulator tests are published as integration/support code with a deliberately missing core. See [third-party notices](../THIRD_PARTY_NOTICES.md).

## Hardware connections

Connect ESP32 **GPIO25** to Feather **RX / GPIO1**, and connect **GND to GND**. The tested setup uses a single UART signal at **230400 baud, 8N1** with 3.3 V logic. Power the boards independently over USB during initial testing; do not assume their power pins should be joined.

Connect the Feather's HSTX connector to the matching 22-pin FPC HSTX-to-DVI adapter, then connect the adapter to a monitor's HDMI input. Check ribbon contact orientation with power disconnected. The adapter carries DVI-compatible video through an HDMI connector; this setup has no audio output.

## Local tested tools

| Tool / library | Local record |
| --- | --- |
| Arduino IDE / Arduino CLI | Windows host; CLI used for repeatable GBA builds |
| Arduino-ESP32 | 3.2.0 compiled the bridge in the retained record |
| HamzaYslmn/esp-ps5 | Required `latestPacket` API; other similarly named libraries are not interchangeable |
| Arduino-Pico, Earle Philhower | 5.4.4 for Feather RP2350 HSTX |
| Adafruit DVI HSTX | 1.2.5 |
| Adafruit GFX | 1.12.4 |
| Python | Python 3 for ROM packing/native test preparation |
| CMake / C++ compiler | CMake 3.25+ and C++20; native tests used MSVC |

These are the local versions used during development, not a claim that newer versions have been tested. Driver clock/IRQ behavior is version-specific. No installed board core or Adafruit library was modified for the port.

## ESP32 sender

Open `DualSenseReader/DualSenseReader.ino` with its `UartProtocol.h` in the same folder. Install the appropriate Espressif ESP32 board package and the [HamzaYslmn library](https://github.com/HamzaYslmn/esp-ps5). Select the ESP32 board and its actual serial port. The original machine used COM4, but that is not a portable setting.

Flash the sketch, then hold Create + PS on the DualSense until it flashes. Reset the ESP32 to start its scan if needed; later use PS to reconnect. Initial pairing can take up to the library's 30-second scan interval. Keep UART TX wired to the Feather; boot/library USB diagnostics are not the game-control protocol.

## Feather input viewer

Install the Arduino-Pico package, select **Adafruit Feather RP2350 HSTX**, and install Adafruit DVI HSTX and Adafruit GFX. Open `FeatherReceiver/FeatherReceiver.ino` with its accompanying headers. This sketch shows the controller independently of the emulator.

For the simplest video isolation test, upload `HDMITest/HDMITest.ino`. The other diagnostic folders are staged memory tests from the historical investigation, not alternate emulator builds. Read their stage descriptions before interpreting a failure.

Use the Feather's port rather than the ESP32 port. If ordinary uploading cannot connect, hold BOOT, tap RESET, then release BOOT. Copy a locally built UF2 for the intended sketch to the bootloader drive. Wait for copying to finish and for the drive to disappear before judging startup.

## Local GBA build: prerequisites intentionally absent here

The local v24 setup used the separately held **modified** core under `GBAPrototype/GBAFeather/src/core/` and `src/gba/`. Those files cannot be recovered from this publication. Copying the unmodified upstream checkout is not enough because the frontend depends on this port's private API and behavior changes. Obtain permission and the corresponding sources, or adapt a licensed core, before expecting an emulator build.

The original build script looks for Arduino CLI at the Windows Arduino IDE installation and uses the user's Arduino IDE YAML configuration. Adjust those installation/config paths for your host. It enables C++20, `-O3`, the RP2350 allocation path, HLE support and the local 16 KiB-stack linker layout. It keeps CPU sources separate; no LTO is enabled.

Once the missing sources are available under suitable terms, the local procedure is:

```powershell
python GBAPrototype/tools/pack_rom.py "C:/path/to/your-authorized-game.gba"
powershell -NoProfile -ExecutionPolicy Bypass -File GBAPrototype/build.ps1 -Clean
```

The packer creates `src/generated/RomIndex.h`, `romblocks.bin`, `rom_blob.S` and `manifest.json`, checks round-trip identity and records CRCs. Regenerate after moving the project because the assembly includes an absolute local asset path. Generated files and all output binaries remain ignored by Git.

The exact board configuration used for the private image is:

```text
rp2040:rp2040:adafruit_feather_rp2350_hstx:psram=8mb,usbstack=nousb
```

The running GBA firmware deliberately has no USB serial port. Use BOOT + RESET to enter the ROM bootloader. `build.ps1` produces local output under `build/gba/` and `GBAPrototype/output/`; neither is published. The full image must fit flash after compression plus firmware and index overhead.

## Native test procedure: also requires the missing core

After generating local ROM assets and restoring permitted matching sources:

```powershell
python GBAPrototype/tests/prepare.py
cmake -S GBAPrototype/tests -B build/gba-host
cmake --build build/gba-host --config Release --parallel 1
$env:GBA_TEST_THREADED='1'
$env:GBA_TEST_SLICED='1'
$env:GBA_TEST_AUTO_INPUT='1'
$env:GBA_TEST_MUTE='1'
$env:GBA_TEST_FRAMES='6000'
build/gba-host/Release/gba_loader_test.exe "C:/path/to/your-authorized-game.gba" GBAPrototype/GBAFeather/src/generated/romblocks.bin build/last-frame.ppm
```

`prepare.py` expands GCC case ranges for MSVC without changing their intended semantics. The test compares original-ROM and compressed-ROM paths. `GBA_TEST_PC_PROFILE=1` additionally enables native guest-instruction sampling, not a board wall-time profiler.

Independent PPU comparison originally used a locally preserved v21 source archive and `tools/prepare_ppu_reference.py`. That archive is not published because it contains the excluded core. The relevant tests cannot be reproduced from the public tree alone. See the retained [validation record](validation.md) rather than interpreting a missing-source error as a packaging accident.
