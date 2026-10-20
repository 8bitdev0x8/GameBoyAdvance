#include <iostream>
#include <random>
#include <vector>
#include "gba/core/ppu.hpp"
#include "gba/core/render_worker.hpp"
#include "ppu_reference.hpp"

int main() {
    constexpr unsigned fixture_count=480;
    std::mt19937 rng(0x235022);
    gba::Ppu optimized;
    gba::Ppu direct;
    gba::ScanlineWorker worker;
    worker.set_profile_enabled(true);
    const auto backend=worker.backend();
    gba::u32 elapsed=0;
    std::vector<gba::u16> display(320*240);
    gba::ReferencePpu reference;
    std::vector<gba::u8> vram(gba::kVramSize),palette(gba::kPaletteSize),oam(gba::kOamSize);
    auto write=[&](unsigned address,unsigned value) {
        optimized.write_register(address,value,gba::BusWidth::Half);
        reference.write_register(address,value,gba::BusWidth::Half);
        direct.write_register(address,value,gba::BusWidth::Half);
    };
    for(unsigned fixture=0;fixture<fixture_count;++fixture) {
        optimized.reset();reference.reset();direct.reset();
        const bool crop=fixture%5==0;
        optimized.set_direct_128x128(crop);reference.set_direct_128x128(crop);
        direct.set_direct_128x128(crop);
        std::fill(display.begin(),display.end(),gba::u16(0xdead));
        for(unsigned y=48;y<208;++y)std::fill_n(display.data()+y*320+40,240,gba::u16(0xffdf));
        worker.set_video_target(crop?nullptr:display.data()+48*320+40,crop?0:320);
        for(auto& b:vram)b=gba::u8(rng());
        for(unsigned i=0;i<palette.size();i+=2) {
            const auto color=rng()&0x7fff;
            palette[i]=gba::u8(color);palette[i+1]=gba::u8(color>>8);
        }
        for(auto& b:oam)b=gba::u8(rng());
        const unsigned mode=fixture%6;
        unsigned control=(rng()&0xff70u)|mode;
        if(fixture%7==0)control=0x1f00|mode; // no windows/effects fast case
        if(fixture>=240)control=0x1f00|mode|(((fixture/4)%8)<<13);
        write(0x04000000,control);
        for(unsigned bg=0;bg<4;++bg) {
            write(0x04000008+bg*2,rng());
            write(0x04000010+bg*4,rng()&511);
            write(0x04000012+bg*4,rng()&511);
        }
        for(unsigned bg=0;bg<2;++bg) {
            const unsigned base=0x04000020+bg*16;
            for(unsigned i=0;i<4;++i)write(base+i*2,rng()&0xffff);
            for(unsigned i=0;i<4;++i)write(base+8+i*2,rng());
        }
        for(unsigned a=0x04000040;a<=0x0400004a;a+=2)write(a,rng());
        write(0x04000050,fixture%7==0?0:rng());
        write(0x04000052,rng());write(0x04000054,rng());
        if(fixture>=240) {
            // Deliberate span edges, wrapped/empty/overlapping windows and all
            // effect modes. Keep forced blank off so these exercise the mixer.
            constexpr unsigned edges[]={0,1,63,127,128,159,160,239,240,241,255};
            write(0x04000040,(edges[fixture%11]<<8)|edges[(fixture+3)%11]);
            write(0x04000042,(edges[(fixture+5)%11]<<8)|edges[(fixture+7)%11]);
            write(0x04000044,fixture%2?0xa001:0x00a0);
            write(0x04000046,fixture%3?0x00a0:0xa028);
            write(0x04000048,((fixture*7)&63)|(((fixture*13)&63)<<8));
            write(0x0400004a,((fixture*11)&63)|(((fixture*17)&63)<<8));
            write(0x04000050,((fixture%4)<<6)|((fixture*3)&63)|(((fixture*5)&63)<<8));
        }
        for(int pass=0;pass<2;++pass) {
            for(int line=0;line<160;++line) {
                if(line==73) {
                    for(unsigned n=0;n<128;++n) {
                        const unsigned offset=rng()%vram.size();
                        vram[offset]=gba::u8(rng());
                        optimized.mark_vram_dirty(offset,1);direct.mark_vram_dirty(offset,1);
                    }
                    reference.mark_memory_dirty(0x06000000);
                }
                if(line==95) {
                    // Cross many cached rows and exercise the large-range epoch fallback.
                    const unsigned offset=fixture%2?0:0x17c00;
                    for(unsigned n=0;n<1024;++n)vram[offset+n]=gba::u8(rng());
                    optimized.mark_vram_dirty(offset,1024);direct.mark_vram_dirty(offset,1024);
                    reference.mark_memory_dirty(0x06000000);
                }
                optimized.render_scanline(line,vram,palette,oam);
                reference.render_scanline(line,vram,palette,oam);
                backend.submit(backend.context,direct,line,vram,palette,oam,&elapsed);
                (void)worker.run_one();worker.wait();
            }
            const auto a=optimized.framebuffer(),b=reference.framebuffer();
            if(!std::equal(a.begin(),a.end(),b.begin(),b.end())) {
                const auto at=std::mismatch(a.begin(),a.end(),b.begin()).first-a.begin();
                std::cerr<<"FAIL fixture "<<fixture<<" mode "<<mode<<" pass "<<pass<<" pixel "<<at<<" new "<<a[at]<<" old "<<b[at]<<"\n";
                return 1;
            }
            if(crop) {
                const auto pixels=direct.framebuffer();
                if(!std::equal(pixels.begin(),pixels.end(),b.begin(),b.end())) {
                    std::cerr<<"FAIL: cropped worker fallback fixture "<<fixture<<"\n";return 1;
                }
            } else {
                for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x) {
                    gba::u16 expected=0xdead;
                    if(y>=48 && y<208 && x>=40 && x<280) {
                        const auto v=b[(y-48)*240+x-40];
                        expected=gba::u16(((v&31)<<11)|(((v>>5)&31)<<6)|((v>>10)&31));
                    }
                    if(display[y*320+x]!=expected) {
                        std::cerr<<"FAIL: direct video fixture "<<fixture<<" pass "<<pass<<" pixel "<<x<<","<<y<<"\n";return 1;
                    }
                }
            }
        }
    }
    const auto profile=worker.profile_snapshot();
    if(!gba::renderProfileConsistent(profile) || profile.total_us!=elapsed ||
       profile.jobs!=fixture_count*2*160 || profile.dirty_jobs!=worker.completed()) {
        std::cerr<<"FAIL: renderer timing partition/job accounting\n";return 1;
    }
    std::cout<<"PASS: profiling-enabled PPU worker stage accounting equals measured Draw time\n";
    std::cout<<"PASS: 480 PPU fixtures match v21 across modes 0-5, all text sizes, flips, affine/sprites, windows, blending, cropped/full output, video writes and repeated frames; includes 240 directed window-span/effect fixtures\n";
    std::cout<<"PASS: direct RGB565 worker output and protected margins match independent v21 pixels; cropped framebuffer fallback also matches\n";
}
