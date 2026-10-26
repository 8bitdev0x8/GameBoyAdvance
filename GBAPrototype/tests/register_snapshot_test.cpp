#include <atomic>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include "gba/core/ppu.hpp"
#include "gba/core/render_worker.hpp"
#include "ppu_reference.hpp"

int main() {
    // Independent pre-optimization PPU renders each line immediately. The new
    // worker must preserve that result even after live registers have changed.
    gba::Ppu queued;
    gba::ReferencePpu reference;
    gba::ScanlineWorker worker;
    worker.set_profile_enabled(true);
    const auto backend=worker.backend();
    queued.set_register_snapshots(true);
    std::vector<gba::u8> vram(gba::kVramSize),palette(gba::kPaletteSize),oam(gba::kOamSize);
    std::vector<gba::u16> display(320*240,0xdead);
    worker.set_video_target(display.data()+48*320+40,320);
    gba::u32 elapsed=0;
    std::mt19937 rng(0x372350);
    auto write=[&](unsigned a,unsigned v,gba::BusWidth w=gba::BusWidth::Half) {
        queued.write_register(a,v,w);reference.write_register(a,v,w);
    };
    std::atomic<bool> stop{false};
    std::thread consumer;
    unsigned lines=0;
    for(unsigned threaded=0;threaded<2;++threaded) {
        stop.store(false,std::memory_order_relaxed);
        if(threaded)consumer=std::thread([&] {
            while(!stop.load(std::memory_order_acquire))
                if(!worker.run_one())std::this_thread::yield();
        });
        for(unsigned fixture=0;fixture<120;++fixture) {
            worker.wait();queued.reset();reference.reset();
            std::fill(display.begin(),display.end(),gba::u16(0xdead));
            for(auto& b:vram)b=gba::u8(rng());
            for(auto& b:palette)b=gba::u8(rng());
            for(auto& b:oam)b=gba::u8(rng());
            const unsigned mode=fixture%6;
            for(unsigned bg=0;bg<4;++bg)write(0x04000008+2*bg,rng());
            for(unsigned a=0x04000020;a<=0x0400003e;a+=2)write(a,rng());
            for(int base=0;base<160;base+=4) {
                for(int line=base;line<base+4;++line) {
                    // Force multiple distinct states to coexist in the queue.
                    // Include forced blank, windows, effects, affine refs and
                    // byte/word merges, with all six graphics modes.
                    write(0x04000000,(rng()&0xfff0)|mode);
                    for(unsigned bg=0;bg<4;++bg)write(0x04000010+4*bg,rng(),gba::BusWidth::Word);
                    write(0x04000028,rng(),gba::BusWidth::Word);
                    write(0x0400003c,rng(),gba::BusWidth::Word);
                    write(0x04000040,rng(),gba::BusWidth::Word);
                    write(0x04000044,rng(),gba::BusWidth::Word);
                    write(0x04000048,rng(),gba::BusWidth::Word);
                    write(0x04000050,rng(),gba::BusWidth::Word);
                    write(0x04000054,rng());
                    write(0x04000051,rng(),gba::BusWidth::Byte);
                    reference.render_scanline(line,vram,palette,oam);
                    backend.submit(backend.context,queued,line,vram,palette,oam,&elapsed);
                    ++lines;
                }
                // Further live writes must not affect any of those four jobs.
                write(0x04000000,0x80);
                write(0x04000010,rng(),gba::BusWidth::Word);
                if(!threaded)for(unsigned j=0;j<4;++j)worker.run_one();
                worker.wait();
                const auto pixels=reference.framebuffer();
                for(int y=base;y<base+4;++y)for(unsigned x=0;x<320;++x) {
                    const auto c=x>=40 && x<280?pixels[y*240+x-40]:0;
                    const auto expected=x>=40 && x<280?
                        gba::u16(((c&31)<<11)|(((c>>5)&31)<<6)|((c>>10)&31)):gba::u16(0xdead);
                    if(display[(y+48)*320+x]!=expected) {
                        std::cerr<<"FAIL: snapshot fixture "<<fixture<<" threaded "<<threaded
                            <<" line "<<y<<" x "<<x<<"\n";
                        stop.store(true,std::memory_order_release);
                        if(consumer.joinable())consumer.join();
                        return 1;
                    }
                }
            }
        }
        stop.store(true,std::memory_order_release);
        if(consumer.joinable())consumer.join();
    }
    const auto profile=worker.profile_snapshot();
    if(!gba::renderProfileConsistent(profile) || profile.total_us!=elapsed ||
       profile.jobs!=lines || profile.dirty_jobs!=worker.completed()) {
        std::cerr<<"FAIL: concurrent renderer timing partition/job accounting\n";return 1;
    }
    std::cout<<"PASS: profiling-enabled concurrent worker counters equal measured Draw time\n";
    std::cout<<"PASS: "<<lines<<" queued register snapshots match independent PPU pixels; "
        "four deferred jobs, concurrent live writes, byte/word merges, all graphics modes and RGB565 margins\n";
}
