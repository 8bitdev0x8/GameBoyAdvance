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

    // Exercise the bus entry point: testing Ppu::write_register alone cannot
    // establish that its caller avoided the cross-core barrier.
    fast.reset(true);plain.reset(true);
    unsigned reference_barriers=0,register_cases=0;
    fast.bus().set_render_barrier([](void* p){++*static_cast<unsigned*>(p);},&barriers);
    plain.bus().set_render_barrier([](void* p){++*static_cast<unsigned*>(p);},&reference_barriers);
    auto register_write=[&](gba::u32 address,gba::u32 value,gba::BusWidth width,int expected_skip=-1) {
        const auto before=barriers,reference_before=reference_barriers;
        if(expected_skip>=0) {
            const auto normalized=width==gba::BusWidth::Word?address&~3u:
                width==gba::BusWidth::Half?address&~1u:address;
            check(fast.ppu().storage_write_unchanged(normalized,value,width)==bool(expected_skip),
                  "Pure register preflight must match effective raw storage changes");
        }
        write(address,value,width,1000000+register_cases++*20);
        check(reference_barriers==reference_before+1,"Reference display write must retain its barrier");
        if(expected_skip>=0)check(barriers==before+unsigned(!expected_skip),
            "Only unchanged pure-storage display writes may bypass the renderer barrier");
        for(gba::u32 a=0x04000000;a<=0x04000054;a+=2)
            check(fast.ppu().read_register(a,gba::BusWidth::Half)==plain.ppu().read_register(a,gba::BusWidth::Half),
                  "Register barrier bypass must preserve all display register readbacks");
    };
    register_write(0x04000000,0x1234,gba::BusWidth::Half,0);
    register_write(0x04000000,0x1234,gba::BusWidth::Half,1);
    register_write(0x04000001,0x12,gba::BusWidth::Byte,1);
    register_write(0x04000001,0x13,gba::BusWidth::Byte,0);
    register_write(0x04000008,0xffffffff,gba::BusWidth::Word,0);
    register_write(0x04000008,0xffffffff,gba::BusWidth::Word,1);
    // BG0CNT read masks bit 13. A same-value byte write must still perform
    // the original masked merge, which changes this raw bit on its first use.
    register_write(0x04000008,0xff,gba::BusWidth::Byte,0);
    register_write(0x04000008,0xff,gba::BusWidth::Byte,1);
    register_write(0x04000048,0xffffffff,gba::BusWidth::Word,0);
    register_write(0x04000048,0xffffffff,gba::BusWidth::Word,1);
    register_write(0x04000048,0xff,gba::BusWidth::Byte,0);
    register_write(0x04000048,0xff,gba::BusWidth::Byte,1);
    register_write(0x04000050,0xffffffff,gba::BusWidth::Word,0);
    register_write(0x04000050,0xffffffff,gba::BusWidth::Word,1);
    register_write(0x04000050,0xff,gba::BusWidth::Byte,0);
    register_write(0x04000050,0xff,gba::BusWidth::Byte,1);
    register_write(0x04000028,0x92345678,gba::BusWidth::Word,0);
    register_write(0x04000028,0x92345678,gba::BusWidth::Word,1);
    register_write(0x0400002b,0x92,gba::BusWidth::Byte,1);
    register_write(0x0400002b,0x12,gba::BusWidth::Byte,0);
    register_write(0x0400002b,0x12,gba::BusWidth::Byte,1);
    // Status, unknown, and partially unknown word writes are conservative.
    register_write(0x04000004,0,gba::BusWidth::Half,0);
    register_write(0x04000004,0,gba::BusWidth::Half,0);
    register_write(0x04000004,0,gba::BusWidth::Word,0);
    register_write(0x04000006,0,gba::BusWidth::Half,0);
    register_write(0x0400004e,0,gba::BusWidth::Half,0);
    register_write(0x0400004c,0,gba::BusWidth::Word,0);
    register_write(0x04000054,0,gba::BusWidth::Word,0);
    {
        const auto before=barriers;
        fast.ppu().optimize_video_writes=false;
        register_write(0x04000054,0,gba::BusWidth::Half);
        check(barriers==before+1,"Disabling PPU video optimization must retain the original barrier");
        fast.ppu().optimize_video_writes=true;
    }
    {
        const auto before=barriers;
        fast.bus().optimize_video_writes=false;
        register_write(0x04000054,0,gba::BusWidth::Half);
        check(barriers==before+1,"Disabling bus video optimization must retain the original barrier");
        fast.bus().optimize_video_writes=true;
    }
    gba::IrqController fast_scan_irq,plain_scan_irq;
    for(unsigned i=0;i<12000;++i) {
        if(i%700==0) {
            const auto cycle=gba::u64(i/700+1)*1232u;
            fast.ppu().advance_to(cycle,fast_scan_irq);
            plain.ppu().advance_to(cycle,plain_scan_irq);
        }
        const auto address=0x04000000u+rng()%0x55u;
        const auto width=i%3==0?gba::BusWidth::Byte:i%3==1?gba::BusWidth::Half:gba::BusWidth::Word;
        const auto value=gba::u32(rng());
        register_write(address,value,width);
        register_write(address,value,width);
    }
    check(fast.ppu().skipped_register_halves>1000,"Repeated display writes must exercise barrier bypass");
    std::cout<<"PASS: "<<register_cases<<" display-register writes preserve cycles/latches/readbacks, byte masked merges and affine halves; unchanged storage skips joins, status/unknown/partial words retain joins\n";
    // Exercise the explicit backend capability. A legacy backend retains all
    // its conservative barriers; only a snapshot-aware backend may defer them.
    fast.set_render_backend({&barriers,nullptr,[](void* p){++*static_cast<unsigned*>(p);},true});
    for(unsigned i=0;i<2000;++i) {
        const auto before=barriers;
        const gba::u32 address=0x04000000+(i%43)*2;
        write(address,gba::u32(rng()),i%2?gba::BusWidth::Word:gba::BusWidth::Byte,2000000+i*20);
        check(barriers==before,"Snapshot-aware display writes must not drain pending jobs");
    }
    for(gba::u32 region:{0x05000000u,0x06000000u,0x07000000u}) {
        const auto old=fast.bus().read(region,gba::BusWidth::Half,gba::AccessType::NonSequential,3000000).value;
        check(plain.bus().read(region,gba::BusWidth::Half,gba::AccessType::NonSequential,3000000).value==old,
              "Reference memory read remains synchronized before barrier check");
        const auto before=barriers;
        write(region,old^0xffffu,gba::BusWidth::Half,3000020);
        check(barriers==before+1,"Snapshot backend must retain changed shared-memory barrier");
    }
    fast.set_render_backend({&barriers,nullptr,[](void* p){++*static_cast<unsigned*>(p);}});
    const auto before=barriers;
    const auto control=fast.ppu().dispcnt();
    write(0x04000000,control^0x80u,gba::BusWidth::Half,4000000);
    check(barriers==before+1,"Switching back to a legacy backend restores register joins");
    std::cout<<"PASS: 2000 snapshot-backend register writes preserve guest bus state without joins; changed VRAM/palette/OAM still drain, legacy backend restores register barrier\n";
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
