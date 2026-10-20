#pragma once
#include "src/gba/core/rom_provider.hpp"
#include "src/generated/RomIndex.h"
#include "src/tinf/tinf.h"
#include <Arduino.h>
#include "src/psram_arena.h"
#include "src/esp_attr.h"

class CompressedRom final : public gba::RomProvider {
  static constexpr uint32_t SLOTS=GBA_ROM_BACKING_BYTES/ROM_PAGE_SIZE;
  static_assert(SLOTS<65535 && ROM_BLOCK_COUNT<65535);
  static constexpr uint32_t HOT_LINE_SIZE=256,HOT_LINES=GBA_ROM_HOT_BYTES/HOT_LINE_SIZE;
  uint8_t* cache=nullptr;
  uint8_t* hot=nullptr;
  mutable uint32_t tags[SLOTS];
  mutable uint16_t pageSlot[ROM_BLOCK_COUNT];
  mutable uint8_t referenced[SLOTS];
  mutable uint32_t clockHand=0;
  mutable uint32_t hotTags[HOT_LINES];
  mutable uint8_t hotVictim[HOT_LINES/2];
  // Separate from evictable data-cache lines. Ordinary data reads may replace
  // hot-cache slots without invalidating the current instruction window.
  alignas(4) mutable uint8_t instructionLine[256]{};
  // Cache-hit callbacks execute beside the interpreter in internal SRAM.
  // Cache fill/decompression remains on the existing flash path.
  static gba::u8 IRAM_ATTR fastByte(const void* p,gba::u32 a) {return static_cast<const CompressedRom*>(p)->read_byte(a);}
  static gba::u16 IRAM_ATTR fast16(const void* p,gba::u32 a) {return static_cast<const CompressedRom*>(p)->read16(a);}
  static gba::u32 IRAM_ATTR fast32(const void* p,gba::u32 a) {return static_cast<const CompressedRom*>(p)->read32(a);}
  static const uint8_t* codeLine(const void* p,gba::u32 base) {
    const auto* self=static_cast<const CompressedRom*>(p);
    if((base&255u) || base>=ROM_SIZE)return nullptr;
    static_assert(HOT_LINE_SIZE==256);
    const uint8_t* source=self->hotAddress(base);
    if(!source)return nullptr;
    memcpy(self->instructionLine,source,256);
    return self->instructionLine;
  }
#ifdef __GNUC__
  __attribute__((noinline))
#endif
  const uint8_t* fillHotLine(uint32_t a,uint32_t line,uint32_t slot) const {
    ++hotMisses;
    const uint8_t* backing=page(a/ROM_PAGE_SIZE);
    if(!backing)return nullptr;
    const uint32_t start=line*HOT_LINE_SIZE;
    const uint32_t count=std::min(HOT_LINE_SIZE,ROM_SIZE-start);
    uint8_t* out=hot+slot*HOT_LINE_SIZE;
    memcpy(out,backing+(start%ROM_PAGE_SIZE),count);
    if(count<HOT_LINE_SIZE)memset(out+count,0xff,HOT_LINE_SIZE-count);
    hotTags[slot]=line;
    hotVictim[slot/2]=(slot&1u)^1u;
    return out+(a%HOT_LINE_SIZE);
  }
  const uint8_t* hotAddress(uint32_t a) const {
    const uint32_t line=a/HOT_LINE_SIZE,set=line&((HOT_LINES/2)-1),base=set*2;
    if(hotTags[base]==line) {
      ++hotHits;hotVictim[set]=1;return hot+base*HOT_LINE_SIZE+(a%HOT_LINE_SIZE);
    }
    if(hotTags[base+1]==line) {
      ++hotHits;hotVictim[set]=0;return hot+(base+1)*HOT_LINE_SIZE+(a%HOT_LINE_SIZE);
    }
    const uint32_t slot=hotTags[base]==0xffffffffu?base:
      hotTags[base+1]==0xffffffffu?base+1:base+hotVictim[set];
    return fillHotLine(a,line,slot);
  }
public:
  mutable uint32_t misses=0,hits=0,inflateUs=0,maxInflateUs=0;
  mutable uint32_t hotHits=0,hotMisses=0;
  mutable bool failed=false;
  CompressedRom() {
    for(auto& tag:tags) tag=0xffffffff;
    for(auto& slot:pageSlot)slot=0xffff;
    for(auto& used:referenced)used=0;
    for(auto& tag:hotTags)tag=0xffffffff;
    for(auto& victim:hotVictim)victim=0;
    cache=static_cast<uint8_t*>(gbaPsramAllocate(SLOTS*ROM_PAGE_SIZE));
    hot=gbaRomHotAllocate();
  }
  ~CompressedRom() override { gbaPsramRelease(cache);gbaRomHotRelease(hot); }
  bool ready() const { return cache && hot; }
  size_t backing_capacity() const { return SLOTS; }
  const uint8_t* cached_page(uint32_t index) const {
    return index<ROM_BLOCK_COUNT && pageSlot[index]!=0xffffu ? cache+pageSlot[index]*ROM_PAGE_SIZE : nullptr;
  }
  size_t size() const override { return ROM_SIZE; }
  gba::RomFastAccess fast_access() const override {
    return {this,&fastByte,&fast16,&fast32,nullptr,0,&codeLine};
  }
  const uint8_t* page(uint32_t index) const {
    if(!cache || index>=ROM_BLOCK_COUNT) { failed=true;return nullptr; }
    uint32_t slot=pageSlot[index];
    if(slot!=0xffffu) { ++hits;referenced[slot]=1;return cache+slot*ROM_PAGE_SIZE; }
    // Fully associative second-chance replacement avoids page-index alias
    // thrashing while retaining O(1) lookup for every cached block.
    while(referenced[clockHand]) {
      referenced[clockHand]=0;
      if(++clockHand==SLOTS)clockHand=0;
    }
    slot=clockHand;if(++clockHand==SLOTS)clockHand=0;
    if(tags[slot]!=0xffffffffu)pageSlot[tags[slot]]=0xffff;
    tags[slot]=0xffffffffu;
    uint8_t* out=cache+slot*ROM_PAGE_SIZE;
    ++misses;
    const RomBlock& b=ROM_BLOCKS[index];
    unsigned length=ROM_PAGE_SIZE;
    uint32_t start=micros();
    int result=TINF_OK;
    if(b.raw) { memcpy(out,gba_rom_packed+b.offset,b.size);length=b.size; }
    else result=tinf_uncompress(out,&length,gba_rom_packed+b.offset,b.size);
    const bool valid=result==TINF_OK && length==b.unpacked && tinf_crc32(out,length)==b.crc;
    const uint32_t elapsed=micros()-start;
    inflateUs+=elapsed;if(elapsed>maxInflateUs)maxInflateUs=elapsed;
    if(!valid) {
      failed=true;tags[slot]=0xffffffff;return nullptr;
    }
    tags[slot]=index;
    pageSlot[index]=uint16_t(slot);referenced[slot]=1;
    return out;
  }
  gba::u8 read_byte(gba::u32 address) const override {
    if(address>=ROM_SIZE)return 0xff;
    const uint8_t* p=hotAddress(address);
    return p?*p:0xff;
  }
  gba::u16 read16(gba::u32 a) const override {
    if((a&(HOT_LINE_SIZE-1))>HOT_LINE_SIZE-2 || a>=ROM_SIZE-1)
      return RomProvider::read16(a);
    const uint8_t* p=hotAddress(a);
    if(!p)return 0xffff;
    uint16_t value;memcpy(&value,p,2);return value;
  }
  gba::u32 read32(gba::u32 a) const override {
    if((a&(HOT_LINE_SIZE-1))>HOT_LINE_SIZE-4 || a>=ROM_SIZE-3)
      return RomProvider::read32(a);
    const uint8_t* p=hotAddress(a);
    if(!p)return 0xffffffff;
    uint32_t value;memcpy(&value,p,4);return value;
  }
  bool read_bytes(gba::u32 a,std::span<gba::u8> out) const override {
    if(a>ROM_SIZE || out.size()>ROM_SIZE-a)return false;
    while(!out.empty()) {
      const uint8_t* p=page(a/ROM_PAGE_SIZE);
      if(!p)return false;
      const size_t offset=a%ROM_PAGE_SIZE;
      const size_t count=std::min(out.size(),size_t(ROM_PAGE_SIZE-offset));
      memcpy(out.data(),p+offset,count);out=out.subspan(count);a+=count;
    }
    return true;
  }
};
