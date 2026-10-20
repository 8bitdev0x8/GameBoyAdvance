#include <iostream>
#include <random>
#include "gba/core/emulator.hpp"
void check(bool,const char*);

void testBiosReads() {
    gba::Emulator fast,plain;
    plain.cartridge().optimize_bios_reads=false;
    std::mt19937 random(0xb1052350);
    unsigned directCases=0,busCases=0;
    for(unsigned size:{0u,1u,2u,3u,4u,7u,255u,16383u,16384u,20000u}) {
        std::vector<gba::u8> image(size);
        for(auto& value:image)value=gba::u8(random());
        fast.load_bios(image);plain.load_bios(image);
        fast.cartridge().set_bios_view(image);
        check(fast.cartridge().bios().size()==std::min(size,16384u),"BIOS load retains 16 KiB clipping");
        check(fast.cartridge().bios().data()==image.data(),"Borrowed BIOS retains external storage without copying");
        for(unsigned index=0;index<20000;++index) {
            const gba::u32 address=index<16400?index:gba::u32(random());
            for(auto width:{gba::BusWidth::Byte,gba::BusWidth::Half,gba::BusWidth::Word}) {
                check(fast.cartridge().read_bios(address,width)==plain.cartridge().read_bios(address,width),
                      "Direct BIOS scalar load matches preserved bounded-byte reference");
                ++directCases;
            }
        }
        for(auto width:{gba::BusWidth::Byte,gba::BusWidth::Half,gba::BusWidth::Word})
        for(gba::u32 address:{0xfffffffcu,0xfffffffdu,0xfffffffeu,0xffffffffu}) {
            check(fast.cartridge().read_bios(address,width)==plain.cartridge().read_bios(address,width),
                  "BIOS out-of-range wrapping addresses stay unmapped");++directCases;
        }
        fast.reset(true);plain.reset(true);
        for(unsigned index=0;index<12000;++index) {
            const auto width=index%3==0?gba::BusWidth::Byte:index%3==1?gba::BusWidth::Half:gba::BusWidth::Word;
            gba::AccessType access=index%4==0?gba::AccessType::CodeFetch:gba::AccessType::NonSequential;
            if(index%2)access|=gba::AccessType::Sequential;
            if(index%5==0)access|=gba::AccessType::CpuOutsideBios;
            if(index%11==0)access|=gba::AccessType::Dma;
            const gba::u32 address=index%13==0?0x00003ffcu+(index%8):random()%0x4000u;
            const auto a=fast.bus().read(address,width,access,index),b=plain.bus().read(address,width,access,index);
            check(a.value==b.value && a.cycles==b.cycles && a.open_bus==b.open_bus &&
                  a.dma_open_bus==b.dma_open_bus && a.breaks_fetch_burst==b.breaks_fetch_burst,
                  "BIOS bus protected/data/DMA/code access result parity");
            check(fast.bus().idle_bus_state()==plain.bus().idle_bus_state(),"BIOS latch/open-bus/prefetch parity");
            ++busCases;
        }
    }
    std::cout<<"PASS: "<<directCases<<" borrowed BIOS scalar reads vs owned preserved byte reader, empty/partial/full/clipped sizes, all alignments, end and 32-bit address bounds; "
             <<busCases<<" bus code/data/DMA/protected reads with exact results/cycles/latches\n";
}
