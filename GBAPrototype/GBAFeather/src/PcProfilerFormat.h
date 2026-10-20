#pragma once
#include <cstddef>
#include <cstdint>

constexpr unsigned GBA_PC_RANGE_CAPACITY=1152;
struct GbaPcRange { uint32_t begin,end; char label[24]; };
static_assert(sizeof(GbaPcRange)==32);
extern const GbaPcRange gbaPcRanges[GBA_PC_RANGE_CAPACITY];
extern const uint32_t gbaPcRangeCount;
#ifdef _MSC_VER
#define GBA_PROFILE_INLINE __forceinline
#else
#define GBA_PROFILE_INLINE inline __attribute__((always_inline))
#endif

// A secure Cortex-M33 frame using default callee stacking has R0..xPSR
// at offsets 0..28. FP storage follows, rather than preceding, this frame.
// Do not guess the layout of nonsecure/cross-security/callee-stacked frames.
GBA_PROFILE_INLINE bool gbaProfileFramePc(
    const uint32_t* frame,uint32_t excReturn,uintptr_t lower,uintptr_t upper,uint32_t& pc) {
  const uintptr_t address=reinterpret_cast<uintptr_t>(frame);
  if((excReturn&0xffffff83u)!=0xffffff81u || (excReturn&0x60u)!=0x60u ||
     (address&3u) || address<lower || upper<32 || address>upper-32)return false;
  const uint32_t xpsr=frame[7];
  if(!(xpsr&(1u<<24)))return false;
  if(bool(excReturn&8u)!=(0u==(xpsr&0x1ffu)))return false;
  pc=frame[6]&~1u;
  return true;
}

GBA_PROFILE_INLINE int gbaProfileFindRange(
    uint32_t pc,const GbaPcRange* ranges,uint32_t count) {
  uint32_t lower=0,upper=count;
  while(lower<upper) {
    const uint32_t mid=lower+(upper-lower)/2;
    if(pc<ranges[mid].begin)upper=mid;else lower=mid+1;
  }
  if(lower && pc<ranges[lower-1].end)return int(lower-1);
  return -1;
}
