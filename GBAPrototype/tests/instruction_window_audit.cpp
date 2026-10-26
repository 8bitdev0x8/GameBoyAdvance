#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include "gba/core/emulator.hpp"
void check(bool,const char*);

class WindowTestRom final:public gba::RomProvider {
    std::vector<gba::u8> bytes_;
    bool enabled_;
    alignas(4) mutable std::array<gba::u8,256> line_{};
    static const gba::u8* line(const void* context,gba::u32 base) {
        const auto* self=static_cast<const WindowTestRom*>(context);
        ++self->calls;
        if(base==self->failBase || base>=self->bytes_.size() || (base&255u))return nullptr;
        self->line_.fill(0xff);
        std::copy_n(self->bytes_.data()+base,std::min(size_t(256),self->bytes_.size()-base),self->line_.data());
        return self->line_.data();
    }
public:
    mutable unsigned calls=0;
    gba::u32 failBase=0xffffffffu;
    explicit WindowTestRom(std::vector<gba::u8> bytes,bool enabled=true):bytes_(std::move(bytes)),enabled_(enabled){}
    size_t size()const override{return bytes_.size();}
    gba::u8 read_byte(gba::u32 address)const override{return address<bytes_.size()?bytes_[address]:0xff;}
    gba::RomFastAccess fast_access()const override {
        gba::RomFastAccess access;access.context=this;access.code_line=enabled_?&line:nullptr;return access;
    }
};

void testInstructionWindow() {
    std::mt19937 random(0xc0de2350);
    std::vector<gba::u8> bytes(0x20003);
    for(auto& byte:bytes)byte=gba::u8(random());
    gba::Cartridge cached,reference;
    auto provider=std::make_unique<WindowTestRom>(bytes);auto* stats=provider.get();
    cached.set_rom_provider(std::move(provider));reference.set_rom_provider(std::make_unique<WindowTestRom>(bytes,false));
    unsigned direct=0;
    for(gba::u32 address=0;address<0x20200;++address)
    for(auto width:{gba::BusWidth::Half,gba::BusWidth::Word}) {
        check(cached.read_instruction(address,width)==reference.read_rom(address,width),"Window values preserve alignment, lines, ROM ends and fallback");++direct;
        const auto fixed=width==gba::BusWidth::Half?cached.read_instruction_fixed<gba::BusWidth::Half>(address):
            cached.read_instruction_fixed<gba::BusWidth::Word>(address);
        check(fixed==reference.read_rom(address,width),"Fixed-width hits preserve alignment, short final lines and bounds");
    }
    const auto filled=stats->calls;
    (void)cached.read_instruction(0x1000,gba::BusWidth::Half);
    const auto atLine=stats->calls;
    for(unsigned i=0;i<1000;++i) {
        check(cached.read_instruction_fixed<gba::BusWidth::Half>(0x1000+(i%128)*2)==reference.read_rom(0x1000+(i%128)*2,gba::BusWidth::Half),"Fixed window hit value parity");
        (void)cached.read_rom((i*123u)%bytes.size(),gba::BusWidth::Byte);
    }
    check(stats->calls==atLine && stats->calls>filled,"Sequential instructions reuse exactly one stable line despite data reads");
    stats->failBase=0x1800;
    check(cached.read_instruction(0x1800,gba::BusWidth::Word)==reference.read_rom(0x1800,gba::BusWidth::Word),"Failed window acquisition uses ordinary cartridge path");
    const auto beforeHeader=stats->calls;
    for(gba::u32 address=0;address<256;++address)(void)cached.read_instruction(address,gba::BusWidth::Half);
    check(stats->calls==beforeHeader,"Header/GPIO line never requests raw instruction storage");
    auto replacement=bytes;for(auto& byte:replacement)byte^=0xa5;
    cached.set_rom_provider(std::make_unique<WindowTestRom>(replacement));
    reference.set_rom_provider(std::make_unique<WindowTestRom>(replacement,false));
    check(cached.instruction_hits()==0 && cached.instruction_fills()==0,"Provider replacement clears pointers/tags/counters");
    check(cached.read_instruction(0x1000,gba::BusWidth::Word)==reference.read_rom(0x1000,gba::BusWidth::Word),"ROM replacement cannot reuse stale instruction bytes");

    gba::Emulator fast,plain;
    fast.set_rom_provider(std::make_unique<WindowTestRom>(bytes));
    plain.set_rom_provider(std::make_unique<WindowTestRom>(bytes,false));
    fast.reset(true);plain.reset(true);
    for(auto* emu:{&fast,&plain}) {
        emu->cartridge().set_rtc_enabled(true);
        (void)emu->bus().write(0x080000c8,1,gba::BusWidth::Half,gba::AccessType::NonSequential,0);
    }
    unsigned busCases=0;
    for(unsigned index=0;index<60000;++index) {
        const auto width=index%97==0?gba::BusWidth::Byte:(index/2000)%2?gba::BusWidth::Word:gba::BusWidth::Half;
        gba::AccessType access=gba::AccessType::CodeFetch;
        if(index%8)access|=gba::AccessType::Sequential;
        if(index%101==0)access|=gba::AccessType::Dma;
        if(index%89==0)access=gba::AccessType::NonSequential;
        gba::u32 address=0x08000000u+(index%0x10000)*gba::u32(width);
        if(index%23==0)address=0x0a000000u+(random()%bytes.size());
        if(index%37==0)address=0x0c000000u+(random()%bytes.size());
        if(index%71==0)address=0x080000c4u+(index%6);
        if(index%79==0)address=0x0801fffcu+(index%8);
        if(index%257==0)for(auto* emu:{&fast,&plain})
            (void)emu->bus().write(0x04000204,index%514?0:0x4317,gba::BusWidth::Half,gba::AccessType::Io,index);
        if(index%5==0){const int cycles=random()%32;fast.bus().prefetch_advance(cycles);plain.bus().prefetch_advance(cycles);}
        const auto expected=plain.bus().read(address,width,access,index);
        const auto got=fast.bus().read_code(address,width,access,index);
        check(gba::u32(got)==expected.value && gba::u32(got>>32)==expected.cycles,"Instruction window bus data/cycle parity");
        check(fast.bus().idle_bus_state()==plain.bus().idle_bus_state(),"Instruction window preserves bus/prefetch/latches across DMA, aliases and GPIO");
        ++busCases;
    }
    check(fast.cartridge().instruction_hits()>1000 && fast.cartridge().instruction_fills()>10,"Bus instruction window must actually be exercised");
    for(auto* emu:{&fast,&plain})emu->cartridge().set_save_type(gba::SaveType::Eeprom);
    const auto fills=fast.cartridge().instruction_fills();
    for(unsigned index=0;index<68;++index) {
        const auto expected=plain.bus().read(0x0d000000,gba::BusWidth::Half,gba::AccessType::CodeFetch,index);
        const auto got=fast.bus().read_code(0x0d000000,gba::BusWidth::Half,gba::AccessType::CodeFetch,index);
        check(gba::u32(got)==expected.value && gba::u32(got>>32)==expected.cycles && fast.bus().idle_bus_state()==plain.bus().idle_bus_state(),"EEPROM code access retains peripheral path");
    }
    check(fast.cartridge().instruction_fills()==fills,"EEPROM does not use raw instruction window");
    std::cout<<"PASS: "<<direct<<" instruction-window alignment/boundary/bounds reads, stable hits/data interleaving, failed-provider/header fallback and ROM replacement; "
             <<busCases<<" mixed bus reads preserve values/cycles/latches/prefetch, aliases/GPIO/DMA/EEPROM\n";
}

void testLeafFetch() {
    using namespace gba;
    std::mt19937 rng(0x1eaf46);
    std::vector<u8> bytes(0x40003);
    for(auto& byte:bytes)byte=u8(rng());
    Emulator fast,plain;
    auto provider=std::make_unique<WindowTestRom>(bytes);auto* source=provider.get();
    fast.set_rom_provider(std::move(provider));
    plain.set_rom_provider(std::make_unique<WindowTestRom>(bytes,false));
    fast.reset(true);plain.reset(true);
    auto write=[&](u32 address,u32 value) {
        for(auto* emu:{&fast,&plain})(void)emu->bus().write(address,value,BusWidth::Half,AccessType::Io,0);
    };
    unsigned hits=0;
    auto fetch=[&](u32 address,BusWidth width,AccessType access,u64 cycle) {
        u32 value=0,cycles=0;bool hit=false;
        if(width==BusWidth::Half)hit=fast.bus().try_code_line<BusWidth::Half>(address,access,value,cycles);
        else hit=fast.bus().try_code_line<BusWidth::Word>(address,access,value,cycles);
        if(!hit) {
            const u64 result=width==BusWidth::Half?fast.bus().read_code_specialized<BusWidth::Half>(address,access,cycle):
                fast.bus().read_code_specialized<BusWidth::Word>(address,access,cycle);
            value=u32(result);cycles=u32(result>>32);
        } else ++hits;
        const auto expected=plain.bus().read(address,width,access,cycle);
        check(value==expected.value && cycles==expected.cycles,"Leaf fetch exact value/cycles");
        check(fast.bus().idle_bus_state()==plain.bus().idle_bus_state(),"Leaf fetch exact prefetch/latches/open bus");
    };
    for(unsigned i=0;i<160000;++i) {
        if(i%4096==0)write(0x04000204,(i/4096)%4?0x4317:0);
        const auto width=(i/8192)%2?BusWidth::Word:BusWidth::Half;
        u32 address=0x08000100u+(i%32768u)*u32(width);
        AccessType access=AccessType::CodeFetch|AccessType::Sequential;
        if(i%257==0)access=AccessType::CodeFetch;
        if(i%263==0)access=AccessType::NonSequential;
        if(i%269==0)access|=AccessType::Dma;
        if(i%101==0)address=0x0a000000u+(rng()&0x3fffc);
        if(i%103==0)address=0x08040000u+(i%4);
        if(i%107==0)address=0x080000c4u;
        if(i%109==0)address=0x03000000u+(rng()&0xffff);
        if(i%113==0)address=0x00000018u;
        if(i%7==0){const int cycles=rng()%16;fast.bus().prefetch_advance(cycles);plain.bus().prefetch_advance(cycles);}
        fetch(address,width,access,i);
    }
    check(hits>5000,"Leaf cache-hit path must be exercised");
    // Exact hint edges and incomplete final line use the complete fetch path.
    for(auto width:{BusWidth::Half,BusWidth::Word})
    for(u32 address:{0x08007efcu,0x08007efeu,0x08007f00u,0x08007ffcu,0x0801fffcu,
                    0x08020000u,0x08040000u,0x08040002u,0x0d000000u})
        fetch(address,width,AccessType::CodeFetch|AccessType::Sequential,200000);
    source->failBase=0x1800;
    fetch(0x08001800,BusWidth::Half,AccessType::CodeFetch,200001);
    for(auto* emu:{&fast,&plain})emu->cartridge().set_save_type(SaveType::Eeprom);
    for(unsigned i=0;i<68;++i)fetch(0x0d000000,BusWidth::Half,AccessType::CodeFetch|AccessType::Sequential,200002+i);
    for(auto& byte:bytes)byte^=0x5a;
    fast.set_rom_provider(std::make_unique<WindowTestRom>(bytes));
    plain.set_rom_provider(std::make_unique<WindowTestRom>(bytes,false));
    fetch(0x08000100,BusWidth::Word,AccessType::CodeFetch|AccessType::Sequential,200100);
    std::cout<<"PASS: 160000 inline-leaf fetch fixtures preserve exact values/cycles/prefetch/latches; cached hits "<<hits
        <<", aliases, hint edges, ROM tail, failed provider, GPIO, RAM, BIOS, DMA, EEPROM and ROM replacement\n";
}
