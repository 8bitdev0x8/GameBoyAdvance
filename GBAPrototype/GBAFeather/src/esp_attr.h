#pragma once
// The Feather linker copies .time_critical sections to internal SRAM at boot.
// Keep desktop tests portable; firmware hot paths must not execute from XIP.
#ifdef GBA_HOST_TEST
#define IRAM_ATTR
#define GBA_HOT_CONST_ATTR
#else
#define IRAM_ATTR __attribute__((section(".time_critical.gba")))
#define GBA_HOT_CONST_ATTR __attribute__((section(".time_critical.gba_const")))
#endif
#define DRAM_ATTR
