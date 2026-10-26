#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include "gba/core/cartridge.hpp"
#include "../GBAFeather/BiosStorage.h"

namespace { bool denyNew=false;unsigned denied=0; }
void* operator new(std::size_t n) {
    if(denyNew){++denied;throw std::bad_alloc();}
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
std::size_t progress=0,totalBytes=0,calls=0;
bool ordered=true;
void callback(std::size_t completed,std::size_t total) {
    ordered=ordered && total==totalBytes && completed>progress && completed<=total;
    progress=completed;++calls;
}
bool contains(const std::vector<gba::u8>& bytes,std::string_view marker) {
    return std::search(bytes.begin(),bytes.end(),marker.begin(),marker.end())!=bytes.end();
}
void check(bool condition,const char* label) {
    if(!condition){denyNew=false;std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}
}
alignas(4) std::array<uint8_t,16384> biosDestination;
unsigned biosRequests=0;
bool rejectBiosAllocation=false;
void* biosAllocate(size_t n) {
    ++biosRequests;
    if(rejectBiosAllocation || n>biosDestination.size())return nullptr;
    return biosDestination.data();
}
void testBiosStorage() {
    alignas(4) std::array<uint8_t,16384> source;
    for(size_t i=0;i<source.size();++i)source[i]=uint8_t(i*37u+11u);
    gba::Cartridge cart;
    denyNew=true;
    const auto lowHeap=prepareBootBios(source,source.size()+BIOS_HEAP_RESERVE_BYTES-1,biosAllocate);
    check(!lowHeap.sramCopy && lowHeap.bytes.data()==source.data() && biosRequests==0,
          "Low heap retains source without attempting allocation");
    rejectBiosAllocation=true;
    const auto fragmented=prepareBootBios(source,100000,biosAllocate);
    check(!fragmented.sramCopy && fragmented.bytes.data()==source.data() && biosRequests==1,
          "Allocator failure retains source without abort");
    cart.set_bios_view(fragmented.bytes);
    check(cart.has_bios() && cart.bios().data()==source.data() && cart.bios().size()==source.size(),
          "Fallback BIOS binds external storage without new/new[]");
    for(unsigned i=0;i<source.size();++i)
        check(cart.read_bios(i,gba::BusWidth::Byte)==source[i],"Every fallback BIOS byte matches selected source");
    rejectBiosAllocation=false;
    const auto copied=prepareBootBios(source,source.size()+BIOS_HEAP_RESERVE_BYTES,biosAllocate);
    check(copied.sramCopy && copied.bytes.data()==biosDestination.data() && biosRequests==2 &&
          std::equal(copied.bytes.begin(),copied.bytes.end(),source.begin()),
          "Enough heap makes exactly one checked, byte-identical SRAM copy");
    cart.set_bios_view(copied.bytes);
    const auto empty=prepareBootBios({},100000,biosAllocate);
    check(empty.bytes.empty() && !empty.sramCopy && biosRequests==2,"Empty BIOS never allocates");
    cart.set_bios_view({});check(!cart.has_bios(),"Empty view clears BIOS");
    denyNew=false;
    cart.set_bios(std::vector<uint8_t>(source.begin(),source.end()));
    check(cart.bios().data()!=source.data(),"Owned BIOS keeps independent storage");
    denyNew=true;
    cart.set_bios_view(source);
    check(cart.bios().data()==source.data(),"Owned-to-borrowed replacement needs no allocation");
    denyNew=false;
    cart.set_bios(std::vector<uint8_t>(source.begin(),source.end()));
    check(cart.bios().data()!=source.data() && cart.read_bios(0,gba::BusWidth::Byte)==source[0],
          "Borrowed-to-owned replacement restores independent storage");
    std::cout<<"PASS: BIOS low-heap and failed-allocation fallback, exact 16 KiB checked SRAM copy, all fallback bytes, empty image and owned/view replacement; no new/new[] while binding fallback\n";
}
}
int main() {
    testBiosStorage();
    const std::array<std::string_view,8> markers={"SRAM_V","EEPROM_V","FLASH1M_V","FLASH512_V","FLASH_V","RTC_V","SIIRTC","IRTC_V"};
    unsigned cases=0;
    for(auto marker:markers)for(unsigned boundary:{1024u,16384u})
    for(unsigned split=0;split<=marker.size();++split)for(bool atEnd:{false,true}) {
        const unsigned start=boundary-split;
        std::vector<gba::u8> bytes(atEnd?start+marker.size():boundary+1027,0);
        std::copy(marker.begin(),marker.end(),bytes.begin()+start);
        const bool rtc=contains(bytes,"RTC_V")||contains(bytes,"SIIRTC")||contains(bytes,"IRTC_V");
        const auto save=contains(bytes,"SRAM_V")?gba::SaveType::Sram:
            contains(bytes,"EEPROM_V")?gba::SaveType::Eeprom:
            contains(bytes,"FLASH1M_V")?gba::SaveType::Flash128K:
            (contains(bytes,"FLASH512_V")||contains(bytes,"FLASH_V"))?gba::SaveType::Flash64K:gba::SaveType::Sram;
        auto provider=std::make_unique<gba::MemoryRomProvider>(std::move(bytes));
        gba::Cartridge cart;cart.set_scan_progress(callback);
        totalBytes=provider->size();progress=0;calls=0;ordered=true;
        denyNew=true;
        try{cart.set_rom_provider(std::move(provider));}
        catch(const std::bad_alloc&){denyNew=false;check(false,"ROM feature scan attempted a heap allocation");}
        denyNew=false;
        check(ordered && progress==totalBytes && calls==(totalBytes+1023)/1024,"Complete ordered 1 KiB scan callbacks including partial tail");
        check(cart.rtc_enabled()==rtc,"RTC marker across boundary matches whole-ROM search");
        const auto previousCalls=calls;
        cart.auto_detect_save_type();
        check(cart.save_type()==save && cart.rtc_enabled()==rtc,"Save/RTC detection matches independent whole-ROM search");
        check(calls==previousCalls,"Cached scan is reused");
        ++cases;
    }
    check(denied==0,"No new/new[] calls during scans");
    std::cout<<"PASS: "<<cases<<" allocation-denied ROM scans; all eight markers split across 1 KiB/16 KiB boundaries and file ends, partial tails, save/RTC results, ordered progress and cached reuse\n";
}
