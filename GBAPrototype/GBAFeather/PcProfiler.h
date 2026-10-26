#pragma once
#include <stdint.h>

struct GbaPcProfileSnapshot {
  bool active=false;
  uint32_t samples=0,unmapped=0,invalid=0,irqSamples=0,handlerUs=0,elapsedMs=0;
  char label[3][24]{};
  uint32_t percent[3]{};
  uint16_t rangeIndex[3]{0xffffu,0xffffu,0xffffu};
};
#ifdef GBA_ENABLE_PC_PROFILE
void gbaPcProfileStart();
// Called from normal core-0 code, never the sampling ISR. The final callback
// has last=true and flushes a partial histogram chunk.
using GbaPcHistogramSink=void (*)(unsigned index,unsigned hits,bool last);
void gbaPcProfileUpdate(GbaPcHistogramSink sink=nullptr);
const GbaPcProfileSnapshot& gbaPcProfileSnapshot();
#else
inline void gbaPcProfileStart(){}
inline void gbaPcProfileUpdate(){}
#endif
