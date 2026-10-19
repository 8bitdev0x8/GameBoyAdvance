#include <iostream>
#include <random>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include "gba/core/emulator.hpp"

void check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }

void testThumbMultiple(unsigned firstLoad=0) {
    gba::Emulator emu;
    std::vector<gba::u8> code(16384);
    for(unsigned load=0;load<2;++load)for(unsigned base=0;base<8;++base)for(unsigned list=1;list<256;++list) {
        const unsigned index=(load*8+base)*255+list-1;
        const gba::u16 opcode=gba::u16(0xc000|(load<<11)|(base<<8)|list);
        std::memcpy(code.data()+0x100+index*2,&opcode,2);
    }
    emu.load_rom(std::move(code));
    unsigned cases=0;
    for(gba::u32 region:{0x02000000u,0x03000000u})
    for(unsigned load=firstLoad;load<2;++load)for(unsigned base=0;base<8;++base)for(unsigned list=1;list<256;++list) {
        emu.reset(true);
        const gba::u32 address=region+0x100;
        const unsigned count=std::popcount(list);
        const gba::u32 final=address+count*4;
        const unsigned first=std::countr_zero(list);
        auto& state=emu.cpu().state();
        for(unsigned r=0;r<8;++r)state.regs[r]=0x41000000u+r*0x101u;
        state.regs[base]=address;
        state.regs[15]=0x08000100u+((load*8+base)*255+list-1)*2;
        state.cpsr=0xa000003f;
        const auto original=state.regs;
        for(unsigned i=0;i<8;++i)
            (void)emu.bus().write(address+i*4,0x91000000u+i,gba::BusWidth::Word,gba::AccessType::NonSequential,0);
        (void)emu.cpu().step();
        unsigned offset=0;
        for(unsigned r=0;r<8;++r)if(list&(1u<<r)) {
            if(load) {
                if(state.regs[r]!=0x91000000u+offset) {
                    std::cerr<<"Thumb LDMIA base r"<<base<<" list "<<std::hex<<list<<" r"<<r<<" got "<<state.regs[r]<<" expected "<<(0x91000000u+offset)<<std::dec<<"\n";
                    throw std::runtime_error("LDMIA must retain a loaded base register instead of overwriting it with writeback");
                }
            } else {
                const auto value=emu.bus().peek_word(address+offset*4);
                const auto expected=r==base && r!=first?final:original[r];
                check(value==expected,"STMIA must store the updated base when it is not first in the register list");
            }
            ++offset;
        }
        if(!load || !(list&(1u<<base)))check(state.regs[base]==final,"Multiple-transfer base writeback");
        check(state.cpsr==0xa000003f,"Multiple-transfer flags unchanged");
        for(unsigned r=0;r<8;++r)
            if(!(list&(1u<<r)) && r!=base)check(state.regs[r]==original[r],"Unlisted registers unchanged");
        ++cases;
    }
    std::cout<<"PASS: "<<cases<<" Thumb LDMIA/STMIA cases, all nonempty lists/base registers, EWRAM/IWRAM, base first/later/absent and flags\n";
}

void testFetchPath() {
    gba::Emulator plain,fast;
    std::vector<gba::u8> code(0x40003);
    std::mt19937 rng(0xfeed2350);
    for(auto& b:code)b=gba::u8(rng());
    plain.load_rom(code);fast.load_rom(code);plain.reset(true);fast.reset(true);
    for(auto* emu:{&plain,&fast}) {
        auto ram=emu->bus().iwram();for(unsigned i=0;i<ram.size();++i)ram[i]=gba::u8(i*37u);
        ram=emu->bus().ewram();for(unsigned i=0;i<ram.size();++i)ram[i]=gba::u8(i*19u);
    }
    for(unsigned i=0;i<30000;++i) {
        if(i%257==0)for(auto* emu:{&plain,&fast})
            (void)emu->bus().write(0x04000204,(i/257)%3==0?0:0x4317,gba::BusWidth::Half,gba::AccessType::Io,i);
        const auto width=i%113==0?gba::BusWidth::Byte:(i/64)%2==0?gba::BusWidth::Half:gba::BusWidth::Word;
        gba::AccessType access=gba::AccessType::CodeFetch;
        if(i%37)access|=gba::AccessType::Sequential;
        if(i%97==0)access=gba::AccessType::NonSequential;
        if(i%101==0)access|=gba::AccessType::Dma;
        gba::u32 address=0x08000000u+(i%16384)*gba::u32(width);
        if(i%3==0)address=0x03000000u+(rng()&0xffff);
        else if(i%17==0)address=0x02000000u+(rng()&0x7ffff);
        else if(i%53==0)address=0x0a000000u+(rng()&0x3ffff);
        else if(i%59==0)address=0x0c000000u+(rng()&0x3ffff);
        else if(i%71==0)address=0x08040002u; // partial final word / beyond ROM
        else if(i%79==0)address=0x04000004u;
        else if(i%83==0)address=0x00000018u;
        else if(i%89==0)address=0x10000000u; // unmapped / open bus
        if(i%5==0) {
            const int cycles=int(rng()%64);
            plain.bus().prefetch_advance(cycles);fast.bus().prefetch_advance(cycles);
        }
        const auto a=plain.bus().read(address,width,access,i);
        const auto packed=fast.bus().read_code(address,width,access,i);
        const gba::BusAccessResult b{gba::u32(packed),gba::u32(packed>>32)};
        check(a.value==b.value && a.cycles==b.cycles,"Fast instruction read value/cycles differ");
        check(plain.bus().idle_bus_state()==fast.bus().idle_bus_state(),"Fast instruction read changes bus/prefetch/latches");
    }
    for(auto* emu:{&plain,&fast}) {
        emu->cartridge().set_rtc_enabled(true);
        (void)emu->bus().write(0x080000c8u,1,gba::BusWidth::Half,gba::AccessType::NonSequential,40000);
    }
    for(auto width:{gba::BusWidth::Half,gba::BusWidth::Word})
    for(gba::u32 address:{0x080000c4u,0x080000c6u,0x080000c8u,0x08007efeu,0x08007f00u,0x08007ffcu,
        0x08008000u,0x0801fffcu,0x08020000u,0x0803fffcu,0x08040000u,0x08040002u,0x0a020000u}) {
        plain.bus().prefetch_advance(128);fast.bus().prefetch_advance(128);
        const auto access=gba::AccessType::CodeFetch|gba::AccessType::Sequential;
        const auto a=plain.bus().read(address,width,access,40000);
        const auto packed=fast.bus().read_code(address,width,access,40000);
        const gba::BusAccessResult b{gba::u32(packed),gba::u32(packed>>32)};
        check(a.value==b.value && a.cycles==b.cycles && plain.bus().idle_bus_state()==fast.bus().idle_bus_state(),"RTC GPIO, prefetch hints, 32/128 KiB boundaries and partial-ROM fetch parity");
    }
    for(auto* emu:{&plain,&fast})emu->cartridge().set_save_type(gba::SaveType::Eeprom);
    for(unsigned i=0;i<68;++i) {
        const auto a=plain.bus().read(0x0d000000u,gba::BusWidth::Half,gba::AccessType::CodeFetch,i);
        const auto packed=fast.bus().read_code(0x0d000000u,gba::BusWidth::Half,gba::AccessType::CodeFetch,i);
        const gba::BusAccessResult b{gba::u32(packed),gba::u32(packed>>32)};
        check(a.value==b.value && a.cycles==b.cycles && plain.bus().idle_bus_state()==fast.bus().idle_bus_state(),"EEPROM code fetch must use ordinary peripheral path");
    }
    check(fast.bus().code_fetch_fast_hits>1000,"Instruction fast path exercised");
    std::cout<<"PASS: 30000 mixed instruction reads preserve exact data/cycles/bus/prefetch/latches, RAM mirrors, ROM aliases/bounds, MMIO/BIOS/open-bus/DMA/byte/EEPROM fallback; fast hits "<<fast.bus().code_fetch_fast_hits<<"\n";
}

#ifdef GBA_TIMER_AUDIT
void testTimerReference();
#endif

void testVideoWrites();
void testVideoDma();
int main(int argc,char**) {
    try {
        testThumbMultiple(argc>1?1:0);testFetchPath();
        testVideoWrites();testVideoDma();
#ifdef GBA_TIMER_AUDIT
        testTimerReference();
#endif
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n";return 1; }
}
