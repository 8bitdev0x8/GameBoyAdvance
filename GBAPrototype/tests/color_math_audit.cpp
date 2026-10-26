#include "gba/core/types.hpp"
#include <algorithm>
#include <iostream>
#include <random>
void check(bool,const char*);
namespace gba {
u16 testPpuAlpha(u16,u16,unsigned,unsigned);
u16 testPpuBrightness(u16,unsigned,bool);
}
void testColorMath() {
    using namespace gba;
    unsigned brightness=0,alpha=0;
    for(u32 color=0;color<65536;++color)for(unsigned evy=0;evy<=16;++evy) {
        const unsigned r=color&31,g=(color>>5)&31,b=(color>>10)&31;
        const u16 light=u16((r+(((31-r)*evy)>>4))|((g+(((31-g)*evy)>>4))<<5)|((b+(((31-b)*evy)>>4))<<10));
        const u16 dark=u16((r-((r*evy)>>4))|((g-((g*evy)>>4))<<5)|((b-((b*evy)>>4))<<10));
        check(testPpuBrightness(u16(color),evy,true)==light && testPpuBrightness(u16(color),evy,false)==dark,"Packed brightness exact channels and bit-15 behavior");
        brightness+=2;
    }
    std::mt19937 rng(0xc01045);
    for(unsigned i=0;i<1000000;++i) {
        const u16 a=u16(rng()),b=u16(rng());const unsigned eva=rng()%17,evb=rng()%17;
        u32 expected=0;
        for(unsigned shift:{0u,5u,10u})expected|=std::min(31u,(((a>>shift)&31u)*eva+((b>>shift)&31u)*evb)>>4)<<shift;
        check(testPpuAlpha(a,b,eva,evb)==expected,"Packed alpha exact rounding and saturation");++alpha;
    }
    std::cout<<"PASS: "<<brightness<<" exhaustive brightness outputs and "<<alpha<<" alpha fixtures preserve channel rounding, saturation, all coefficients and bit-15 behavior\n";
}
