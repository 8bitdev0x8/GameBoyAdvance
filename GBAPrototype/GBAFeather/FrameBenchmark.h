#pragma once
#include <stdint.h>

// Deterministic input is indexed by completed guest frames, never wall time.
// Policy 0 is zero input (Sonic stops at language selection); policy 1 matches
// the legacy host probe. Policy 2 selects English and releases all controls.
// Policy 3 registers a name, skips the opening story and lets the title demo run.
namespace FrameBenchmark {
inline uint16_t keys(uint32_t frame,unsigned policy) {
  if(policy==0)return 0;
  if(policy==2)return frame>=300 && frame<305?1u:0u;
  if(policy==3) {
    if((frame>=300 && frame<305)||(frame>=600 && frame<605)||(frame>=1000 && frame<1005))return 1;
    if((frame>=800 && frame<805)||(frame>=1200 && frame<1205))return 8;
    return 0;
  }
  uint16_t value=0;
  if(frame>=300 && frame<305)value=1;
  if(frame>=800 && frame<805)value=1u<<3;
  if(frame>=1200 && frame%300<8)value|=1;
  if(frame>=2400 && frame<2408)value=1u<<3;
  if(frame>=3600)value|=1u<<4;
  return value;
}
enum class Phase : uint32_t {Disabled=0,Warmup=1,Measuring=2,Complete=3};
struct Window {
  uint32_t warmup,count,policy;
  Phase phase=Phase::Warmup;
  uint32_t startState=0,endState=0,startVideo=0,endVideo=0;
  uint64_t startCycle=0,endCycle=0;
  bool begins(uint32_t completed) const {return phase==Phase::Warmup && completed==warmup;}
  bool ends(uint32_t completed) const {return phase==Phase::Measuring && completed-warmup==count;}
  bool frozen() const {return phase==Phase::Complete;}
};
inline uint32_t hashByte(uint32_t hash,uint8_t value) {return (hash^value)*16777619u;}
inline uint32_t hashWord(uint32_t hash,uint32_t value) {
  for(unsigned i=0;i<4;++i)hash=hashByte(hash,uint8_t(value>>(i*8)));
  return hash;
}
}
