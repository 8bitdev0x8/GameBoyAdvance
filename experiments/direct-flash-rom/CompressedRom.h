#pragma once
#include "src/gba/core/rom_provider.hpp"
#include "src/generated/RomIndex.h"
#include "src/tinf/tinf.h"
#include <algorithm>
#include <cstring>
// Historical class name retained for existing HUD/startup diagnostics.
// Entire immutable original ROM lives directly in Flash, without RAM copies.
class CompressedRom final:public gba::RomProvider {
public:
 mutable uint32_t misses=0,hits=0,inflateUs=0,maxInflateUs=0,hotHits=0,hotMisses=0;
 mutable bool failed=false;
 bool ready()const{return true;}
 bool native_code_immutable()const override{return true;}
 size_t backing_capacity()const{return ROM_BLOCK_COUNT;}
 size_t size()const override{return ROM_SIZE;}
 const uint8_t* page(uint32_t i)const {if(i>=ROM_BLOCK_COUNT){failed=true;return nullptr;}return gba_rom_packed+i*ROM_PAGE_SIZE;}
 const uint8_t* cached_page(uint32_t i)const{return i<ROM_BLOCK_COUNT?gba_rom_packed+i*ROM_PAGE_SIZE:nullptr;}
 std::span<const gba::u8> direct_read_span()const override{return {gba_rom_packed,ROM_SIZE};}
 static const uint8_t* codeLine(const void*,uint32_t base) {
  if((base&255u)||base>ROM_SIZE||ROM_SIZE-base<256u)return nullptr;
  return gba_rom_packed+base;
 }
 gba::RomFastAccess fast_access()const override{return {nullptr,nullptr,nullptr,nullptr,gba_rom_packed,ROM_SIZE,&codeLine};}
 gba::u8 read_byte(gba::u32 a)const override{return a<ROM_SIZE?gba_rom_packed[a]:0xff;}
 gba::u16 read16(gba::u32 a)const override{if(a>=ROM_SIZE-1)return RomProvider::read16(a);uint16_t v;memcpy(&v,gba_rom_packed+a,2);return v;}
 gba::u32 read32(gba::u32 a)const override{if(a>=ROM_SIZE-3)return RomProvider::read32(a);uint32_t v;memcpy(&v,gba_rom_packed+a,4);return v;}
 bool read_bytes(gba::u32 a,std::span<gba::u8> out)const override{if(a>ROM_SIZE||out.size()>ROM_SIZE-a)return false;memcpy(out.data(),gba_rom_packed+a,out.size());return true;}
};
