#include <iostream>
#include <random>
#include <vector>
#include "gba/core/ppu.hpp"
#include "ppu_reference.hpp"

int main() {
    std::mt19937 rng(0x235022);
    gba::Ppu optimized;
    gba::ReferencePpu reference;
    std::vector<gba::u8> vram(gba::kVramSize),palette(gba::kPaletteSize),oam(gba::kOamSize);
    auto write=[&](unsigned address,unsigned value) {
        optimized.write_register(address,value,gba::BusWidth::Half);
        reference.write_register(address,value,gba::BusWidth::Half);
    };
    for(unsigned fixture=0;fixture<240;++fixture) {
        optimized.reset();reference.reset();
        const bool crop=fixture%5==0;
        optimized.set_direct_128x128(crop);reference.set_direct_128x128(crop);
        for(auto& b:vram)b=gba::u8(rng());
        for(unsigned i=0;i<palette.size();i+=2) {
            const auto color=rng()&0x7fff;
            palette[i]=gba::u8(color);palette[i+1]=gba::u8(color>>8);
        }
        for(auto& b:oam)b=gba::u8(rng());
        const unsigned mode=fixture%6;
        unsigned control=(rng()&0xff70u)|mode;
        if(fixture%7==0)control=0x1f00|mode; // no windows/effects fast case
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
        for(int pass=0;pass<2;++pass) {
            for(int line=0;line<160;++line) {
                if(line==73) {
                    for(unsigned n=0;n<128;++n)vram[rng()%vram.size()]=gba::u8(rng());
                    optimized.mark_memory_dirty(0x06000000);reference.mark_memory_dirty(0x06000000);
                }
                optimized.render_scanline(line,vram,palette,oam);
                reference.render_scanline(line,vram,palette,oam);
            }
            const auto a=optimized.framebuffer(),b=reference.framebuffer();
            if(!std::equal(a.begin(),a.end(),b.begin(),b.end())) {
                const auto at=std::mismatch(a.begin(),a.end(),b.begin()).first-a.begin();
                std::cerr<<"FAIL fixture "<<fixture<<" mode "<<mode<<" pass "<<pass<<" pixel "<<at<<" new "<<a[at]<<" old "<<b[at]<<"\n";
                return 1;
            }
        }
    }
    std::cout<<"PASS: 240 randomized PPU fixtures match v21 across modes 0-5, all text sizes, flips, affine/sprites, windows, blending, cropped/full output, video writes and repeated frames\n";
}
