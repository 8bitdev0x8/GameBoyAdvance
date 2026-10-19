# Direct PSRAM / allocator diagnostic v5

The v4 test lost HDMI after STAGE 1 (pmalloc). That call first initializes TLSF control structures in external memory, so it does not distinguish a heap bug from inaccessible external memory. This test reserves a 2 MiB `.psram` NOLOAD array through the linker and avoids pmalloc until direct memory access has been checked.

Stage 1 reads one word. Stage 2 writes and reads two words. Stages 3/4 write and verify a pattern across the reserved 2 MiB. Stage 5 calls pmalloc(4096) in the separate remaining PSRAM heap. An allocator-free read/write pass followed by stage 5 failure would specifically implicate heap initialization/allocation; earlier failures implicate direct memory access or its configuration.

Each stage is visible for two seconds before its operation. The test arms a one-second watchdog around the operation and records its stage in watchdog scratch registers. If the operation stalls or faults, the board should restart and leave `PREVIOUS TEST CRASHED AT` on HDMI. A HardFault hook separately marks CPU faults. No serial monitor connection is required. If recovery cannot reach HDMI, use the last visible stage.

CPU/video clock arrangement is the same as v4: CPU 120 MHz from PLL_USB, video uses PLL_SYS, PSRAM clock 30 MHz. Firmware does not change installed libraries or core files. The test is compiled with 8 MiB PSRAM and verifies the link map places the reserved array at 0x11000000, with the heap above it. An 8 MiB detection result is still not proof of read/write integrity.

The historical local image was `PSRAMDirectTest-v5.uf2`; compiled outputs are not published here. Build this sketch locally, flash its UF2 using BOOT + RESET, then record PASS or the saved failure stage/type. This is a diagnostic, not a confirmed repair.
