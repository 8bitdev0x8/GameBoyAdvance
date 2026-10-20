#include <iostream>
#include <cstring>
#include "gba/core/emulator.hpp"

void check(bool,const char*);
void testThumbAluFast() {
    gba::Emulator reference,optimized,decoded;
    std::vector<gba::u8> rom(0x8200);
    for(unsigned opcode=0;opcode<0x4000;++opcode) {
        const gba::u16 value=gba::u16(opcode);
        std::memcpy(rom.data()+0x100+opcode*2,&value,2);
    }
    for(auto* emulator:{&reference,&optimized}) {
        emulator->load_rom(rom);emulator->reset(true);
        emulator->cpu().set_fast_fetch(true);
    }
    reference.cpu().optimize_thumb_alu=false;
    const gba::u32 values[]={0,1,0xffffffffu,0x80000000u,0x7fffffffu,0x40000000u,
        0x80000001u,0x55555555u,0xaaaaaaaau,31,32,255,0xffffff00u,0x01000000u,0x7fffff00u,0xdeadbeefu};
    unsigned cases=0;
    for(unsigned opcode=0;opcode<0x4000;++opcode)for(unsigned flags=0;flags<16;++flags) {
        gba::CpuState state;
        for(unsigned reg=0;reg<15;++reg)state.regs[reg]=values[(flags+reg+opcode/64)%16];
        state.regs[15]=0x08000100u+opcode*2;state.cpsr=(flags<<28)|0x3fu;
        state.next_fetch_access=gba::AccessType::CodeFetch;
        for(auto* emulator:{&reference,&optimized}) {
            emulator->cpu().state()=state;emulator->cpu().set_current_cycle(0);
            emulator->cpu().cpu_run_until(1);
        }
        if(reference.cpu().state()!=optimized.cpu().state()) {
            std::cerr<<"ALU opcode "<<std::hex<<opcode<<" flags "<<flags<<std::dec<<"\n";
            check(false,"Fast Thumb ALU register/flag mismatch");
        }
        check(reference.cpu().current_cycle()==optimized.cpu().current_cycle(),"Fast Thumb ALU cycle mismatch");
        check(reference.bus().idle_bus_state()==optimized.bus().idle_bus_state(),"Fast Thumb ALU fetch/latch/prefetch mismatch");
        decoded.cpu().state()=state;
        check(decoded.cpu().execute_decoded_for_test(gba::u16(opcode)),"Decoded ALU accepts each supported encoding");
        decoded.cpu().state().regs[15]=reference.cpu().state().regs[15];
        decoded.cpu().state().next_fetch_access=reference.cpu().state().next_fetch_access;
        check(decoded.cpu().state()==reference.cpu().state(),"Predecoded ALU register/banked/CPSR flags match ordinary decoder");
        ++cases;
    }
    check(optimized.cpu().fast_thumb_alu_count==cases && reference.cpu().fast_thumb_alu_count==0,"Both ALU decoder paths exercised independently");
    std::cout<<"PASS: "<<cases<<" Thumb fast and predecoded ALU cases: every 0x0000-0x3fff encoding, all NZCV inputs, edge operands, register aliases, exact CPU/cycles/bus parity; decoded register/CPSR parity\n";
}
