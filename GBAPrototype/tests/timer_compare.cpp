#include <iostream>
#include <random>
#include <stdexcept>
#include <tuple>
#include "gba/core/timers.hpp"
#include "gba/core/irq.hpp"
#include "gba/core/apu.hpp"
#include "timers_reference.hpp"

void testTimerReference() {
    gba::Timers fast;gba::ReferenceTimers original;
    gba::IrqController fastIrq,oldIrq;gba::Apu fastApu,oldApu;
    fast.reset();original.reset();fastIrq.reset();oldIrq.reset();fastApu.reset();oldApu.reset();
    fastApu.set_synthesis_enabled(false);oldApu.set_synthesis_enabled(false);
    fastApu.write_register(0x04000084,0x80,gba::BusWidth::Half,0);
    oldApu.write_register(0x04000084,0x80,gba::BusWidth::Half,0);
    fastApu.write_register(0x04000082,0x730f,gba::BusWidth::Half,0);
    oldApu.write_register(0x04000082,0x730f,gba::BusWidth::Half,0);
    std::mt19937 rng(0x235027);
    gba::u64 now=0;
    const auto channelState=[](const auto& c) {
        return std::tuple(c.reload,c.pending_reload,c.control,c.pending_control,c.counter,c.running,
            c.stop_read_bias,c.pending_reload_valid,c.pending_control_valid,c.timestamp_started,
            c.next_event_cycle,c.reload_apply_cycle,c.control_apply_cycle,c.shift,c.mask);
    };
    const auto require=[](bool ok,const char* message) { if(!ok)throw std::runtime_error(message); };
    for(unsigned i=0;i<60000;++i) {
        // Every prescaler/cascade setting, delayed control/reload writes,
        // disable/re-enable and same-cycle writes occur across the sequence.
        now+=rng()%9;
        if(i%3!=0) {
            const unsigned channel=rng()%4;
            const auto width=i%11==0?gba::BusWidth::Byte:i%7==0?gba::BusWidth::Word:gba::BusWidth::Half;
            const gba::u32 address=0x04000100u+channel*4+(i%2==0?0:2)+(width==gba::BusWidth::Byte?(rng()%2):0);
            gba::u32 value=i%2==0?(0xff00u|rng()%256):(rng()&0xc7u);
            if(width==gba::BusWidth::Word)value=(rng()&0xc7u)<<16|(0xff00u|rng()%256);
            fast.write_register(address,value,width,now,fastIrq,fastApu);
            original.write_register(address,value,width,now,oldIrq,oldApu);
        }
        if(i%5!=0) {
            fast.advance_to(now,fastIrq,fastApu);original.advance_to(now,oldIrq,oldApu);
        }
        require(fast.next_event_cycle()==original.next_event_cycle(),"Cached timer deadline differs from v26");
        for(unsigned channel=0;channel<4;++channel) {
            require(channelState(fast.channels()[channel])==channelState(original.channels()[channel]),"Timer channel differs from v26");
            const gba::u32 address=0x04000100u+channel*4;
            require(fast.read_register(address,gba::BusWidth::Word,now)==original.read_register(address,gba::BusWidth::Word,now),"Timer register read differs from v26");
            require(fast.read_word_register_split(address,now,now+1)==original.read_word_register_split(address,now,now+1),"Split-cycle timer read differs from v26");
        }
        fastIrq.advance(now);oldIrq.advance(now);
        require(fastIrq.iflags()==oldIrq.iflags() && fastIrq.next_event_cycle()==oldIrq.next_event_cycle(),"Timer IRQ timing differs from v26");
        require(fastApu.take_fifo_request_a()==oldApu.take_fifo_request_a() && fastApu.take_fifo_request_b()==oldApu.take_fifo_request_b(),"Timer FIFO requests differ from v26");
    }
    std::cout<<"PASS: 60000 randomized timer operations match independent v26 channels/registers/deadlines/IRQ/FIFO state\n";
}
