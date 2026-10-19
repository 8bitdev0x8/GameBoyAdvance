# Staged HDMI / PSRAM test v4

This isolates the reported video loss after startup in both the GBA build and the v2 memory diagnostic. It contains no emulator, ROM, UART bridge, or decompressor.

The installed HSTX driver runs the CPU from PLL_USB at 240 MHz and configures the separate PLL_SYS for video. This test divides only the CPU clock to 120 MHz, preserving the independent video clock. PSRAM timing is explicitly set for 30 MHz with conservative receive timing. No external libraries or board core files are modified.

On HDMI it holds each stage for three seconds before executing it:

- STAGE 0: video only.
- STAGE 1: about to call pmalloc for 2 MiB. The allocated address is printed if successful.
- STAGE 2: write a changing pattern into the allocation in small chunks.
- STAGE 3: read and compare all words using ordinary mapped volatile pointers.
- PASS counter: repeat with another pattern.

The LED is on at stages 0/2 and off at 1/3. USB serial at 115200 mirrors stage messages without waiting for a connection. There is no claim of success until the board displays PASS. If the picture disappears, report the last visible stage (or the last serial message).

The historical local image was `HDMIPSRAMSafeTest-v4.uf2`; compiled outputs are not published here. Build this sketch using the Adafruit Feather RP2350 HSTX board with PSRAM=8MByte and Adafruit DVI HSTX/GFX libraries, then flash the resulting local UF2 via BOOT + RESET.
