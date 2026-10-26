#include "PcProfilerFormat.h"
#include <array>
#include <iostream>
#include <stdexcept>

void check(bool valid,const char* why){if(!valid)throw std::runtime_error(why);}
int main() {
  try {
    std::array<uint32_t,64> storage{};
    auto* frame=storage.data()+4;
    const auto lower=reinterpret_cast<uintptr_t>(storage.data()),upper=lower+sizeof(storage);
    frame[6]=0x20001234;frame[7]=1u<<24;
    uint32_t pc=0;
    for(uint32_t exceptionReturn:{0xfffffff9u,0xfffffffdu,0xffffffe9u,0xffffffedu})
      check(gbaProfileFramePc(frame,exceptionReturn,lower,upper,pc)&&pc==0x20001234,"secure thread MSP/PSP and FP frame PC");
    frame[6]|=1;
    check(gbaProfileFramePc(frame,0xfffffff9,lower,upper,pc)&&pc==0x20001234,"clear PC Thumb tag");
    frame[7]|=18;
    check(gbaProfileFramePc(frame,0xfffffff1,lower,upper,pc),"preempted secure IRQ frame");
    check(!gbaProfileFramePc(frame,0xfffffff9,lower,upper,pc),"reject thread/exception status mismatch");
    frame[7]=1u<<24;
    for(uint32_t bad:{0u,0x12345679u,0xffffffb9u,0xffffffd9u,0xfffffffbu,0xfffffff8u})
      check(!gbaProfileFramePc(frame,bad,lower,upper,pc),"reject invalid or cross-security return layout");
    check(!gbaProfileFramePc(reinterpret_cast<uint32_t*>(lower-4),0xfffffff9,lower,upper,pc),"reject below SRAM without dereference");
    check(!gbaProfileFramePc(reinterpret_cast<uint32_t*>(upper-28),0xfffffff9,lower,upper,pc),"reject truncated frame without dereference");
    check(!gbaProfileFramePc(reinterpret_cast<uint32_t*>(lower+1),0xfffffff9,lower,upper,pc),"reject unaligned frame");
    frame[7]=0;
    check(!gbaProfileFramePc(frame,0xfffffff9,lower,upper,pc),"reject non-Thumb xPSR");
    const GbaPcRange ranges[]={{0x10000000,0x10000010,"one"},{0x10000020,0x10000030,"two"},{0x20000000,0x20000008,"SRAM"}};
    unsigned tested=0;
    for(unsigned pcValue=0x0ffffffe;pcValue<0x10000040;++pcValue) {
      int expected=-1;for(int i=0;i<3;++i)if(pcValue>=ranges[i].begin&&pcValue<ranges[i].end)expected=i;
      check(gbaProfileFindRange(pcValue,ranges,3)==expected,"range boundaries/gaps match linear reference");++tested;
    }
    check(gbaProfileFindRange(0x20000007,ranges,3)==2,"SRAM code lookup");
    check(gbaProfileFindRange(0xffffffff,ranges,3)==-1,"unknown address");
    check(gbaProfileFindRange(0,ranges,0)==-1,"empty first-pass table");
    std::cout<<"PASS: Cortex-M33 secure thread/IRQ, MSP/PSP, basic/FP frame PC, invalid frame rejection\n";
    std::cout<<"PASS: binary PC lookup vs linear reference across "<<tested<<" addresses, gaps, bounds, SRAM and empty table\n";
  }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
