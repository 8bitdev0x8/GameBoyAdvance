# Feather RP2350 HDMI connector test

Standalone Arduino sketch for the Adafruit Feather RP2350 **with HSTX**. This replaces the UART receiver while loaded; the ESP32 is not needed for this test.

1. Connect the Feather's HSTX FPC connector to an Adafruit 22-pin FPC HSTX-to-DVI adapter using its matching ribbon cable. Connect the adapter to a monitor's HDMI input and select that input. Seat the ribbon with power disconnected, following the adapter's contact orientation.
2. In Arduino IDE use the Earle Philhower Raspberry Pi Pico/RP2040/RP2350 core and board **Adafruit Feather RP2350 HSTX**. Select the Feather's USB port, not ESP32 COM4.
3. Install **Adafruit DVI HSTX** and **Adafruit GFX Library** using Library Manager if needed. They are already installed on this computer.
4. Open `HDMITest.ino`, upload, and power the Feather over USB. No Serial Monitor connection is required.

Expected image: title, eight color bars, grayscale ramp, white outer border, moving green square, and advancing uptime/frame counter. The framebuffer is 320x240 RGB565, doubled to a 640x480 60 Hz video signal. The onboard LED blinks slowly during normal operation; rapid blinking indicates display initialization failed. Initialization success alone does not prove the monitor received the signal; confirm the picture visually.

The library configures the CPU/video clocks; the driver used for the later emulator builds sets the CPU to 240 MHz. Clock behavior depends on the installed driver version. It sends DVI-compatible video over the HDMI connector; this sketch does not test audio. If there is no signal, verify HDMI input, cable, adapter, and ribbon seating/orientation. If the edges are cropped, disable TV overscan or select its PC/Just Scan mode. Alternative resolutions require matching the library's supported timing and adjusting this sketch's layout.

Hardware reference: https://learn.adafruit.com/adafruit-rp2350-22-pin-fpc-hstx-to-dvi-adapter
Library: https://github.com/adafruit/Adafruit-DVI-HSTX
