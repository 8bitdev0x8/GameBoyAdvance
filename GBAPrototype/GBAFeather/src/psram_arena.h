#pragma once
#ifndef GBA_PSRAM_ARENA_H
#define GBA_PSRAM_ARENA_H
#include <stddef.h>
#include <stdint.h>

// Fixed-lifetime arena: no metadata is stored in external memory.
// Allocations live until board reset; individual release is intentionally a no-op.
class GbaArena {
  uint8_t* base_;
  size_t size_,used_=0;
public:
  GbaArena(uint8_t* base,size_t size):base_(base),size_(size){}
  void* allocate(size_t n) {
    if(!n || n>size_ || used_>size_)return nullptr;
    const size_t aligned=(used_+15u)&~size_t(15u);
    if(aligned>size_ || n>size_-aligned)return nullptr;
    void* p=base_+aligned;used_=aligned+n;return p;
  }
  bool owns(const void* p) const {
    const uintptr_t address=reinterpret_cast<uintptr_t>(p);
    const uintptr_t start=reinterpret_cast<uintptr_t>(base_);
    return address>=start && address-start<size_;
  }
  size_t used() const {return used_;}
};
void* gbaPsramAllocate(size_t bytes);
void gbaPsramRelease(void* p);
bool gbaPsramOwns(const void* p);
size_t gbaPsramUsed();
constexpr size_t GBA_FAST_BUS_BYTES=32768+98304+1024+1024+64;
constexpr size_t GBA_ROM_HOT_BYTES=32*1024;
constexpr size_t GBA_PSRAM_ARENA_BYTES=7680*1024; // Leave 512 KiB to the SDK.
// Leave room for the largest cartridge save without consuming scarce SRAM.
constexpr size_t GBA_SAVE_CAPACITY=128*1024;
constexpr size_t GBA_APU_MIX_BYTES=4096*sizeof(int16_t);
#if defined(GBA_PLATFORM_RP2350) && !defined(GBA_APU_INTERNAL_MIX_BUFFER)
// One 16 KiB ROM backing page funds the cold 8 KiB PCM buffer plus margin.
// Keep the full 128 KiB save budget when users replace the cartridge ROM.
constexpr size_t GBA_ROM_BACKING_BYTES=7*1024*1024-GBA_SAVE_CAPACITY-16*1024;
#else
constexpr size_t GBA_ROM_BACKING_BYTES=7*1024*1024-GBA_SAVE_CAPACITY;
#endif
void* gbaCoreBufferAllocate(size_t bytes);
bool gbaFastOwns(const void* p);
uint8_t* gbaRomHotAllocate();
void gbaRomHotRelease(void* p);
#endif
