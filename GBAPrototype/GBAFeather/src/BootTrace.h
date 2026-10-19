#pragma once
#include <stdint.h>
// Optional platform hook: native tests leave this unset.
inline void (*gbaBootTraceHook)(uint32_t)=nullptr;
inline void gbaBootTrace(uint32_t step) { if(gbaBootTraceHook)gbaBootTraceHook(step); }
inline const char* gbaBootStepName(uint32_t step) {
  switch(step) {
    case 20:return "20: ALLOCATE CORE OBJECT SRAM";
    case 21:return "21: ALLOCATE PPU FRAMEBUFFER";
    case 22:return "22: CLEAR PPU FRAMEBUFFER";
    case 23:return "23: PPU CONSTRUCTOR COMPLETE";
    case 24:return "24: ALLOCATE EWRAM 256 KiB";
    case 25:return "25: CLEAR EWRAM 256 KiB";
    case 26:return "26: IWRAM 32 KiB STATIC SRAM";
    case 27:return "27: CLEAR IWRAM 32 KiB";
    case 28:return "28: ALLOCATE PALETTE/OAM";
    case 29:return "29: CLEAR PALETTE/OAM";
    case 30:return "30: VRAM 96 KiB STATIC SRAM";
    case 31:return "31: CLEAR VRAM 96 KiB";
    case 32:return "32: ALLOCATE WAIT TABLES";
    case 33:return "33: BUS CONSTRUCTOR COMPLETE";
    case 34:return "34: CORE CONSTRUCTOR COMPLETE";
    case 35:return "35: OBJECT SRAM ALLOCATED";
    case 36:return "36: CORE RETURNED TO SETUP";
    default:return "UNKNOWN CONSTRUCTOR STEP";
  }
}
