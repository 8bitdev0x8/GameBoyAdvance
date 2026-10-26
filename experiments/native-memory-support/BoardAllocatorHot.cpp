#include "NativeAllocator.h"
#include "HotMemory.h"
#include <stdlib.h>
#ifdef GBA_NATIVE_ALLOCATOR_HOST
extern "C" void* gba_test_psram_alloc(size_t);
extern "C" void gba_test_psram_free(void*);
#else
#include <Arduino.h>
#endif
namespace {
void* externalAlloc(void*,size_t bytes){
#ifdef GBA_NATIVE_ALLOCATOR_HOST
  return gba_test_psram_alloc(bytes);
#else
  return pmalloc(bytes); // Never malloc, even if PSRAM allocation fails.
#endif
}
void externalFree(void*,void* pointer){
#ifdef GBA_NATIVE_ALLOCATOR_HOST
  gba_test_psram_free(pointer);
#else
  free(pointer); // Arduino linker --wrap=free routes pmalloc pointers to PSRAM.
#endif
}
NativeMemory::Allocator allocator;
bool configured=false;
}
bool gbaBoardAllocatorConfigure(const NativeMemory::Config& config){
  const bool ok=allocator.configure(config,{nullptr,externalAlloc,externalFree});if(ok)configured=true;return ok;
}
extern "C" void* gba_board_alloc(size_t bytes,size_t alignment){
  if(!configured && !gbaBoardAllocatorConfigure({}))return nullptr;
  return allocator.allocate(bytes,alignment);
}
extern "C" void gba_board_free(void* pointer,size_t bytes,size_t alignment){const uint32_t hot=gba_board_region_try_free(pointer,bytes,alignment);if(hot==GBA_HOT_OK)return;if(hot==GBA_HOT_INVALID||!allocator.release(pointer,bytes,alignment))gba_board_panic();}
extern "C" [[noreturn]] void gba_board_panic(){abort();}
NativeMemory::Stats gbaBoardAllocatorStats(){return allocator.stats();}
