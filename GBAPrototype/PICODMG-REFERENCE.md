# PicoDMG reference review — 19 October 2026

The [PicoDMG README](https://github.com/dodger-one/PicoDMG) reports a goal of
59.75 FPS met 99% of the time. It targets the original monochrome Game Boy
(DMG) using Peanut-GB, rather than Game Boy Advance. The claim is an author
report; the inspected material does not establish an independent benchmark
across a defined game suite and duration.

The author's [performance article](https://github.com/dodger-one/PicoDMG/blob/master/docs/_posts/2026-06-19-episode07-the-search-for-speed-part1.md)
describes improving SPI throughput, assigning LCD submission to core 1,
queuing scanlines, batching scaled rows into DMA bands, measuring separate
performance buckets, and overclocking to close the remaining gap. It also
documents that frame skip and interlace trade away rendering work; neither
is equivalent to producing complete new frames at the reported rate.

The [current build configuration](https://github.com/dodger-one/PicoDMG/blob/master/src/CMakeLists.txt)
defaults interlace ON and sets the PLL to 1596 MHz / 6 / 1 = 266 MHz. Actual
build flags can override both. This does not prove that the README's reported
run used those exact settings.

The [frontend source](https://github.com/dodger-one/PicoDMG/blob/master/src/firmware/main.c)
counts completed guest frames and prints frameskip/interlace state. Its
scanline callback copies 160 completed pixel values into owned queue slots;
core 1 then submits them to the LCD. That contrasts with our GBA worker,
which does the actual PPU rendering on core 1 and still reads shared VRAM,
palette and OAM. Register snapshots alone do not eliminate those dependencies.

The [ST7789 driver](https://github.com/dodger-one/PicoDMG/blob/master/src/firmware/mk_st7789.c)
batches and sends LCD data using DMA. Our HSTX output has a different scanout
driver and continuously maintained timing; its SPI implementation cannot be
substituted directly. Useful transferable ideas are owned immutable jobs,
transport batching and separate timing buckets. Some are already present in
our project. Its claimed DMG performance does not establish that our current
GBA interpreter can reach 60 FPS.

No PicoDMG code has been copied into the emulator. Downloaded reference
sources are kept privately under `build/references/picodmg/` for inspection.
