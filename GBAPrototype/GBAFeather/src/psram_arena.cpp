#include "psram_arena.h"
#include <stdlib.h>
#ifdef GBA_HOST_TEST
// Tests instantiate multiple emulators concurrently. Exercise the bounded arena
// separately, while providing ordinary allocations for those desktop instances.
void* gbaPsramAllocate(size_t n) {return malloc(n);}
void gbaPsramRelease(void* p) {free(p);}
bool gbaPsramOwns(const void*) {return false;}
size_t gbaPsramUsed() {return 0;}
void* gbaCoreBufferAllocate(size_t n) {return malloc(n);}
bool gbaFastOwns(const void*) {return false;}
uint8_t* gbaRomHotAllocate() {return static_cast<uint8_t*>(malloc(GBA_ROM_HOT_BYTES));}
void gbaRomHotRelease(void* p) {free(p);}
#else
#include <Arduino.h>
alignas(16) PSRAM uint8_t gbaPsramStorage[GBA_PSRAM_ARENA_BYTES];
// The controller/cursor lives in internal SRAM, not the failing PSRAM heap.
static GbaArena arena(gbaPsramStorage,sizeof(gbaPsramStorage));
// Single-emulator lifetime: static SRAM avoids the internal heap path which
// previously failed during construction. Large EWRAM/framebuffer stay in PSRAM.
alignas(16) static uint8_t fastBusStorage[GBA_FAST_BUS_BYTES];
alignas(16) static uint8_t romHotStorage[GBA_ROM_HOT_BYTES];
static GbaArena fastBus(fastBusStorage,sizeof(fastBusStorage));
void* gbaPsramAllocate(size_t n) {return arena.allocate(n);}
void gbaPsramRelease(void*) {} // Never invoke TLSF/free on reserved PSRAM.
bool gbaPsramOwns(const void* p) {return arena.owns(p);}
size_t gbaPsramUsed() {return arena.used();}
void* gbaCoreBufferAllocate(size_t n) {
  if(n==32768 || n==98304 || n==1024 || n==64)return fastBus.allocate(n);
  return gbaPsramAllocate(n);
}
bool gbaFastOwns(const void* p) {return fastBus.owns(p);}
uint8_t* gbaRomHotAllocate() {return romHotStorage;}
void gbaRomHotRelease(void*) {}
#endif
