#pragma once
#include <stddef.h>
#include <stdint.h>
#ifndef GBA_NATIVE_INTERNAL_CAPACITY
#define GBA_NATIVE_INTERNAL_CAPACITY (16u*1024u)
#endif

// Original board glue. Single core0 executor, no allocation from IRQ/core1.
namespace NativeMemory {
struct Config {
  size_t internalBudget=GBA_NATIVE_INTERNAL_CAPACITY;
  size_t smallLimit=4096;
  size_t psramBudget=2u*1024u*1024u;
  bool spillSmallToPsram=true; // explicit routing policy, never PSRAM->malloc
};
struct Backend {
  void* context=nullptr;
  void* (*allocate)(void*,size_t)=nullptr;
  void (*release)(void*,void*)=nullptr;
};
struct Stats {
  size_t internalPayload=0,internalCharged=0,internalPeak=0;
  size_t psramPayload=0,psramCharged=0,psramPeak=0;
  size_t active=0,peakActive=0;
  uint32_t allocations=0,frees=0,internalAllocations=0,psramAllocations=0;
  uint32_t smallSpills=0,invalidRequests=0,budgetFailures=0,backendFailures=0,badFrees=0;
};
class Allocator {
  struct Block {size_t bytes;Block* next;};
  struct Header {
    uint32_t magic,origin;
    void* raw;
    size_t rawBytes,bytes,alignment,charged;
    uintptr_t cookie;
  };
  static constexpr uint32_t Magic=0x4742414e;
  alignas(16) unsigned char pool_[GBA_NATIVE_INTERNAL_CAPACITY]{};
  Block* free_=nullptr;
  Config config_{0,0,0,false}; // keep static pool in BSS; configure before use
  Backend backend_{};
  Stats stats_{};
  bool initialized_=false;
  void* internalRaw(size_t wanted,size_t& charged);
  void releaseInternal(void* raw,size_t charged);
  static uintptr_t cookie(void* user,const Header& h);
public:
  bool configure(Config config,Backend backend); // no active allocations allowed
  void* allocate(size_t bytes,size_t alignment);
  bool release(void* user,size_t bytes,size_t alignment);
  const Stats& stats()const{return stats_;}
  const Config& config()const{return config_;}
  bool ownsInternal(const void* p)const;
  size_t largestInternalFree()const;
  static constexpr size_t headerBytes(){return sizeof(Header);}
};
// Arithmetic planning only: actual firmware linker/stack/USB headroom must be
// checked independently. These bytes are NOT secretly allocated a second time.
struct Reservation {
  size_t legacyHstx=153600,core1Stack=8192,mainStack=16384,usb=16384,romHot=32768;
  size_t total()const{return legacyHstx+core1Stack+mainStack+usb+romHot;}
};
}

extern "C" void* gba_board_alloc(size_t bytes,size_t alignment);
extern "C" void gba_board_free(void* pointer,size_t bytes,size_t alignment);
extern "C" [[noreturn]] void gba_board_panic();
// Configure before creating Rust runtime, query only when allocation is idle.
bool gbaBoardAllocatorConfigure(const NativeMemory::Config& config);
NativeMemory::Stats gbaBoardAllocatorStats();
