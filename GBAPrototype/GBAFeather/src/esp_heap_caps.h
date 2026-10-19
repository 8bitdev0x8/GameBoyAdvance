#pragma once
// RP2350 compatibility adapter for the upstream embedded allocation path.
#include <stdlib.h>
#include <stdint.h>
#include "psram_arena.h"
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
#define MALLOC_CAP_DMA 8
inline void* heap_caps_malloc(size_t n,unsigned caps) {
  void* p=(caps&MALLOC_CAP_SPIRAM)?gbaCoreBufferAllocate(n):malloc(n);
  if(!p && !(caps&MALLOC_CAP_INTERNAL)) p=gbaPsramAllocate(n);
  if(!p) abort(); // Never let the interpreter dereference a failed allocation.
  return p;
}
inline void* heap_caps_aligned_alloc(size_t alignment,size_t n,unsigned caps) {
  (void)alignment; // All requests in this core are 4-byte aligned.
  return heap_caps_malloc(n,caps);
}
inline void heap_caps_free(void* p) {
#ifdef GBA_HOST_TEST
  free(p);
#else
  if(!gbaPsramOwns(p) && !gbaFastOwns(p))free(p);
#endif
}
