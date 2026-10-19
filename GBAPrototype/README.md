# GBA Feather experiment, v24

The [main README](../README.md) is the project overview and current status. Detailed design is in [architecture](../docs/architecture.md), [history](../docs/development-history.md), [debugging](../docs/debugging.md), [validation](../docs/validation.md) and [building](../docs/building.md).

Latest hardware report: Sonic Advance 3 at about **5 FPS**, **VBskip 15-21%**, **Draw 46 ms/frame**, **Copy 10 ms/frame**. The target remains 30-60 FPS; the current build does not meet it. Video output is 640x480 at 60 Hz, independently of emulated frame rate.

This directory contains the v24 frontend and original support code. The modified `gba-embedded` core is deliberately excluded because upstream has not granted a redistribution licence. Generated ROM assets and ROM-containing firmware are also excluded. A fresh clone cannot build the complete GBA firmware or native emulator tests. See [third-party notices](../THIRD_PARTY_NOTICES.md).

The standalone ESP32 bridge, controller viewer and hardware diagnostic sketches elsewhere in the repository do not require the excluded emulator core. They are the independently buildable parts of this publication.
