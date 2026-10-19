#include <iostream>
#include <random>
#include <tuple>
#include "gba/core/emulator.hpp"
void check(bool,const char*);

void testVideoWrites() {
    gba::Emulator fast,plain;
    plain.bus().optimize_video_writes=false;plain.ppu().optimize_video_writes=false;
    std::mt19937 rng(0x235028);
    unsigned barriers=0;
    fast.bus().set_render_barrier([](void* p){++*static_cast<unsigned*>(p);},&barriers);
    auto write=[&](gba::u32 address,gba::u32 value,gba::BusWidth width,unsigned cycle) {
        const auto a=fast.bus().write(address,value,width,gba::AccessType::Sequential,cycle);
        const auto b=plain.bus().write(address,value,width,gba::AccessType::Sequential,cycle);
        check(a.value==b.value && a.cycles==b.cycles && a.open_bus==b.open_bus &&
              a.dma_open_bus==b.dma_open_bus && a.breaks_fetch_burst==b.breaks_fetch_burst,"Video write guest timing/result parity");
        check(fast.bus().idle_bus_state()==plain.bus().idle_bus_state(),"Video write bus/latch parity");
        const auto x=fast.bus().read(address,width,gba::AccessType::NonSequential,cycle+8);
        const auto y=plain.bus().read(address,width,gba::AccessType::NonSequential,cycle+8);
        check(x.value==y.value && x.cycles==y.cycles,"Video mirrored/aligned read parity");
    };
    write(0x05000000,0x1234,gba::BusWidth::Half,0);
    const auto joined=barriers;
    write(0x05000000,0x1234,gba::BusWidth::Half,20);
    check(barriers==joined,"Unchanged memory write must not drain pending renderer");
    write(0x05000000,0x4321,gba::BusWidth::Half,40);
    check(barriers==joined+1,"Changed memory write must drain renderer exactly once");
    for(unsigned i=0;i<30000;++i) {
        const gba::u32 region=0x05000000u+(i%3)*0x01000000u;
        const gba::u32 offset=(rng()&0x00ffffffu);
        const auto width=i%3==0?gba::BusWidth::Byte:i%3==1?gba::BusWidth::Half:gba::BusWidth::Word;
        const auto value=gba::u32(rng());
        write(region+offset,value,width,i*20+60);
        if(i%2==0)write(region+offset,value,width,i*20+70);
    }
    check(std::equal(fast.bus().vram().begin(),fast.bus().vram().end(),plain.bus().vram().begin()),"VRAM complete mirrored/byte-write memory parity");
    check(std::equal(fast.bus().palette().begin(),fast.bus().palette().end(),plain.bus().palette().begin()),"Palette full memory parity");
    check(std::equal(fast.bus().oam().begin(),fast.bus().oam().end(),plain.bus().oam().begin()),"OAM full memory parity");
    fast.ppu().reset();
    for(gba::u32 a=0x04000000;a<=0x04000054;a+=2) {
        if(a==0x04000004 || a==0x04000006)continue;
        const gba::u16 value=gba::u16(rng());
        fast.ppu().write_register(a,value,gba::BusWidth::Half);
        for(int line=0;line<160;++line)fast.ppu().clear_dirty(line);
        fast.ppu().write_register(a,value,gba::BusWidth::Half);
        for(int line=0;line<160;++line)check(!fast.ppu().is_dirty(line),"Unchanged display register must leave clean rows clean");
    }
    std::cout<<"PASS: 45003 randomized/repeated video writes and reads preserve cycles/latches/memory, mirrors/alignment, byte replication/ignored writes; unchanged-write barrier and display-register dirty-state checks\n";
}

struct VideoFixture {
    gba::Cartridge cart;
    gba::Ppu ppu;
    gba::Timers timers;
    gba::DmaEngine dma;
    gba::Apu apu;
    gba::IrqController irq;
    gba::Bus bus{cart,ppu,timers,dma,apu,irq};
};
void testVideoDma() {
    VideoFixture fast,plain;
    plain.bus.optimize_video_writes=false;plain.ppu.optimize_video_writes=false;
    std::vector<gba::u8> rom(262144);
    for(unsigned i=0;i<rom.size();++i)rom[i]=gba::u8(i*29u);
    fast.cart.set_rom(rom);plain.cart.set_rom(rom);
    std::mt19937 rng(0xd3a23528);
    for(unsigned test=0;test<600;++test) {
        for(auto* f:{&fast,&plain}) {
            f->dma.reset();f->irq.reset();f->ppu.reset();f->bus.reset();
            auto ram=f->bus.ewram();for(unsigned i=0;i<ram.size();++i)ram[i]=gba::u8(i*13);
            ram=f->bus.iwram();for(unsigned i=0;i<ram.size();++i)ram[i]=gba::u8(i*17);
        }
        const unsigned width=test%2?4:2,mode=(test/2)%4;
        const gba::u32 source=test%3==0?0x08000000:test%3==1?0x02000000:0x03000000;
        const unsigned offset=test%7==0?0x17ff0u:(rng()%0x18000u)&~3u;
        const gba::u32 destination=0x06000000+offset;
        const unsigned count=1+rng()%512;
        const unsigned ctl=0xc000u|(width==4?0x400u:0u)|(mode<<5);
        for(auto* f:{&fast,&plain}) {
            f->dma.write_register(0x040000d4,source,gba::BusWidth::Word,0);
            f->dma.write_register(0x040000d8,destination,gba::BusWidth::Word,0);
            f->dma.write_register(0x040000dc,count|(ctl<<16),gba::BusWidth::Word,0);
        }
        const auto a=fast.dma.service_due(2,fast.bus,fast.irq),b=plain.dma.service_due(2,plain.bus,plain.irq);
        check(a==b && fast.irq.iflags()==plain.irq.iflags(),"VRAM DMA exact cycles/IRQ parity");
        check(fast.bus.idle_bus_state()==plain.bus.idle_bus_state(),"VRAM DMA bus/latch parity");
        check(std::equal(fast.bus.vram().begin(),fast.bus.vram().end(),plain.bus.vram().begin()),"VRAM DMA bytes parity");
        const auto& x=fast.dma.channels()[3];const auto& y=plain.dma.channels()[3];
        check(std::tie(x.current_source,x.current_destination,x.current_count,x.bus_latch,x.control,x.pending,x.activation_cycle)==
              std::tie(y.current_source,y.current_destination,y.current_count,y.bus_latch,y.control,y.pending,y.activation_cycle),"VRAM DMA final channel parity");
    }
    std::cout<<"PASS: 600 VRAM DMA fixtures from ROM/EWRAM/IWRAM, half/word, destination increment/decrement/fixed/reload, mirrors/end crossing; exact cycles, IRQs, bus/latches, channel state and bytes\n";
}
