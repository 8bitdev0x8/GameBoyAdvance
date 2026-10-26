#include <cstring>
#include <iostream>
#include <stdexcept>
#include "RenderProfileDisplay.h"

static void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
int main() {
    try {
        const gba::RenderProfile before{{0xfffffff0u,4000,100,0xffffff00u,1000},
            11,0xffffff00u,0xfffffff0u,9};
        const gba::RenderProfile now{{14,124000,70100,49744,9000},2011,249774,304,328};
        const auto p=gba::renderProfileDelta(now,before);
        check(p.stages.setup_us==30 && p.stages.background_us==120000 &&
            p.stages.object_us==70000 && p.stages.composite_us==50000 &&
            p.stages.rgb_us==8000 && p.other_us==2000 && p.total_us==250030 &&
            p.jobs==320 && p.dirty_jobs==319,"Modulo-wrap interval deltas");
        check(gba::renderProfileConsistent(p),"Valid partition");
        auto invalid=p;invalid.other_us=0xfffffff0u;
        check(!gba::renderProfileConsistent(invalid),"Reject underflowed residual");
        invalid=p;invalid.dirty_jobs=321;
        check(!gba::renderProfileConsistent(invalid),"Reject excess dirty jobs");
        check(profileTenthsMs(1000,0)==0 && profileTenthsMs(0xffffffffu,0xffffffffu)==0,
            "Zero denominator and wide multiplication");
        char row[80];
        formatRenderWorkRow(row,sizeof(row),p,10,314);
        check(std::strcmp(row,"C1 BG12.0 OBJ7.0 Mix5.0 #314")==0,"Work row units/sample");
        formatRenderSetupRow(row,sizeof(row),p,10,314);
        check(std::strcmp(row,"Prep0.0 RGB0.8 Other0.2 #314")==0,"Setup row units/sample");
        formatRenderWaitRow(row,sizeof(row),80000,80000,0,10,314);
        check(std::strcmp(row,"W8.0 Q8.0 B0.0 ms/f #314")==0,"Wait row units/sample");
        char bounded[5]={'x','x','x','x','!'};
        formatRenderWorkRow(bounded,4,p,10,314);
        check(bounded[3]=='\0' && bounded[4]=='!',"Bounded row formatting");
        std::cout<<"PASS: render-stage delta wrap, partition validation, guarded units and matching-sample HUD strings\n";
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
