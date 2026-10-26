#include <iostream>
#include <random>
#include <cstring>
#include "gba/core/emulator.hpp"
void check(bool,const char*);
namespace {
struct Machine {
    gba::Cartridge cart;
    gba::IrqController irq;
    gba::Ppu ppu;
    gba::Apu apu;
    gba::Timers timers;
    gba::DmaEngine dma;
    gba::Bus bus{cart,ppu,timers,dma,apu,irq};
    gba::Arm7tdmi cpu{bus,irq};
    void start(std::vector<gba::u8> rom,bool blocks) {
        cart.set_rom(std::move(rom));
        std::vector<gba::u8> bios(16384);
        const gba::u32 armNop=0xe1a00000,irqReturn=0xe25ef004;
        for(unsigned i=0;i<bios.size();i+=4)std::memcpy(bios.data()+i,&armNop,4);
        std::memcpy(bios.data()+0x18,&irqReturn,4);
        cart.set_bios(std::move(bios));
        irq.reset();ppu.reset();apu.reset();timers.reset();dma.reset();bus.reset();cpu.reset(true);
        cpu.set_fast_fetch(true);cpu.set_block_execution(blocks);cpu.set_alu_burst(false);
    }
};
class MutableRom final:public gba::RomProvider {
public:
    std::vector<gba::u8> bytes;
    explicit MutableRom(std::vector<gba::u8> b):bytes(std::move(b)){}
    size_t size()const override{return bytes.size();}
    gba::u8 read_byte(gba::u32 a)const override{return a<bytes.size()?bytes[a]:0xff;}
};
}
void runExecutionAudit(bool useBlock) {
    using namespace gba;
    std::mt19937 rng(0xb10c44);
    std::vector<u8> rom(0x20200);
    for(unsigned i=0;i<rom.size();i+=2) {const u16 op=u16(rng()&0x3fff);std::memcpy(rom.data()+i,&op,2);}
    Machine plain,blocks;
    plain.start(rom,false);blocks.start(rom,useBlock);blocks.cpu.set_alu_burst(!useBlock);
    auto compare=[&]() {
        check(plain.cpu.state()==blocks.cpu.state(),"Block CPU/banked/flags state parity");
        check(plain.cpu.current_cycle()==blocks.cpu.current_cycle(),"Block exact guest-cycle parity");
        check(plain.bus.idle_bus_state()==blocks.bus.idle_bus_state(),"Block fetch/prefetch/latch parity");
        check(plain.irq.ie()==blocks.irq.ie() && plain.irq.iflags()==blocks.irq.iflags() &&
            plain.irq.line_asserted()==blocks.irq.line_asserted() && plain.irq.next_event_cycle()==blocks.irq.next_event_cycle(),"Block IRQ state/deadline parity");
        check(plain.dma.next_event_cycle()==blocks.dma.next_event_cycle(),"Block DMA deadline parity");
        check(std::equal(plain.bus.ewram().begin(),plain.bus.ewram().end(),blocks.bus.ewram().begin()),"Block DMA RAM parity");
        const auto& a=plain.dma.channels();const auto& b=blocks.dma.channels();
        for(unsigned i=0;i<4;++i)check(a[i].source==b[i].source && a[i].destination==b[i].destination &&
            a[i].control==b[i].control && a[i].pending==b[i].pending && a[i].activation_cycle==b[i].activation_cycle &&
            a[i].current_source==b[i].current_source && a[i].current_destination==b[i].current_destination &&
            a[i].current_count==b[i].current_count && a[i].bus_latch==b[i].bus_latch,"Block complete DMA state parity");
    };
    auto write=[&](u32 address,u32 value,BusWidth width) {
        for(auto* m:{&plain,&blocks})(void)m->bus.write(address,value,width,AccessType::Io,m->cpu.current_cycle());
    };
    const unsigned slices[]={1,2,3,4,5,7,16,31,63,127,1024};
    unsigned cases=0;
    for(unsigned run=0;run<320;++run) {
        CpuState state;for(auto& r:state.regs)r=rng();
        state.regs[15]=0x08000100+(rng()%0x1f000u&~1u);state.cpsr=(rng()&0xf0000000u)|0x3fu;
        state.next_fetch_access=AccessType::CodeFetch;
        plain.cpu.state()=state;blocks.cpu.state()=state;
        plain.cpu.set_current_cycle(0);blocks.cpu.set_current_cycle(0);
        plain.dma.reset();blocks.dma.reset();plain.irq.reset();blocks.irq.reset();plain.timers.reset();blocks.timers.reset();
        write(0x04000204,run%3?0x4317:0,BusWidth::Half);
        write(0x04000200,0x108,BusWidth::Half);write(0x04000208,1,BusWidth::Half);
        write(0x04000100,0xfff0,BusWidth::Half);write(0x04000102,0xc0,BusWidth::Half);
        if(run&1) {
            write(0x040000b0,0x03001000,BusWidth::Word);write(0x040000b4,0x02001000,BusWidth::Word);
            write(0x040000b8,8,BusWidth::Half);write(0x040000ba,0xe400,BusWidth::Half);
            plain.dma.request_hblank(17);blocks.dma.request_hblank(17);
        }
        for(unsigned n=0;n<64;++n) {
            const u64 target=plain.cpu.current_cycle()+slices[(n+run)%11];
            plain.cpu.cpu_run_until(target);blocks.cpu.cpu_run_until(target);compare();++cases;
            if(n%16==15)write(0x04000202,0x3fff,BusWidth::Half);
        }
    }
    const auto stats=blocks.cpu.block_stats();
    if(useBlock)check(stats.batches>0 && stats.instructions>stats.batches,"Actual multi-instruction decoded blocks exercised");
    else check(blocks.cpu.burst_instructions>0 && blocks.cpu.burst_exits>0,"Actual uncached ALU bursts exercised");
    check(stats.mismatches==0,"Immutable ROM blocks retain opcode validation");
    // Replace ROM without CPU reset: each cached opcode must be validated.
    std::vector<u8> simple(512);
    for(unsigned i=0;i<simple.size();i+=2) {const u16 op=0x3001;std::memcpy(simple.data()+i,&op,2);}
    auto a=std::make_unique<MutableRom>(simple),b=std::make_unique<MutableRom>(simple);
    auto* pa=a.get();auto* pb=b.get();plain.cart.set_rom_provider(std::move(a));blocks.cart.set_rom_provider(std::move(b));
    CpuState state;state.regs[15]=0x08000100;state.cpsr=0x3f;state.next_fetch_access=AccessType::CodeFetch;
    for(auto* m:{&plain,&blocks}) {m->cpu.state()=state;m->cpu.set_current_cycle(0);m->irq.reset();m->dma.reset();m->timers.reset();}
    plain.cpu.cpu_run_until(40);blocks.cpu.cpu_run_until(40);compare();
    const u16 branch=0xe000;std::memcpy(pa->bytes.data()+0x104,&branch,2);std::memcpy(pb->bytes.data()+0x104,&branch,2);
    for(auto* m:{&plain,&blocks}) {m->cpu.state()=state;m->cpu.set_current_cycle(0);}
    plain.cpu.cpu_run_until(40);blocks.cpu.cpu_run_until(40);compare();
    if(useBlock)check(blocks.cpu.block_stats().mismatches>0,"Changed opcode exits decoded block and executes generic branch");
    // A repeating mixed sequence exercises exiting the burst on loads/stores
    // and conditional/unconditional branches, with exact RAM and bus checks.
    std::vector<u8> mixed(512);const u16 sequence[]={0x3001,0x6048,0x684a,0x1c13,0x2bff,0xd1f9,0x2000,0xe7f7};
    std::memcpy(mixed.data()+0x100,sequence,sizeof(sequence));
    plain.start(mixed,false);blocks.start(mixed,useBlock);blocks.cpu.set_alu_burst(!useBlock);
    for(auto* m:{&plain,&blocks}){m->cpu.state().regs[15]=0x08000100;m->cpu.state().regs[1]=0x02001000;m->cpu.state().cpsr=0x3f;}
    for(unsigned n=0;n<2000;++n){const auto target=plain.cpu.current_cycle()+slices[n%11];plain.cpu.cpu_run_until(target);blocks.cpu.cpu_run_until(target);compare();}
    std::cout<<"PASS: "<<cases<<" "<<(useBlock?"decoded-block":"uncached-burst")<<" slice fixtures plus 2000 mixed-instruction slices preserve exact CPU/cycles/prefetch/latches/IRQ/DMA/RAM; prefetch on/off, timer/IRQ entry, mid-block DMA and changed ROM opcode fallback\n";
}
void testBlockExecution(){runExecutionAudit(true);runExecutionAudit(false);}
