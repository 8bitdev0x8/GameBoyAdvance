#ifndef RP2350_GBA_RENDER_PROFILE_DISPLAY_H
#define RP2350_GBA_RENDER_PROFILE_DISPLAY_H
#include <cstdio>
#include "gba/core/render_profile.hpp"

inline uint32_t profileTenthsMs(uint32_t us,uint32_t frames) {
  return frames?uint32_t(uint64_t(us)/(uint64_t(frames)*100u)):0;
}
inline void formatRenderWorkRow(char* out,size_t size,const gba::RenderProfile& p,
    uint32_t frames,uint32_t sample) {
  const auto bg=profileTenthsMs(p.stages.background_us,frames);
  const auto obj=profileTenthsMs(p.stages.object_us,frames);
  const auto mix=profileTenthsMs(p.stages.composite_us,frames);
  snprintf(out,size,"C1 BG%lu.%lu OBJ%lu.%lu Mix%lu.%lu #%03lu",
      (unsigned long)(bg/10),(unsigned long)(bg%10),(unsigned long)(obj/10),
      (unsigned long)(obj%10),(unsigned long)(mix/10),(unsigned long)(mix%10),(unsigned long)sample);
}
inline void formatRenderSetupRow(char* out,size_t size,const gba::RenderProfile& p,
    uint32_t frames,uint32_t sample) {
  const auto prep=profileTenthsMs(p.stages.setup_us,frames);
  const auto rgb=profileTenthsMs(p.stages.rgb_us,frames);
  const auto other=profileTenthsMs(p.other_us,frames);
  snprintf(out,size,"Prep%lu.%lu RGB%lu.%lu Other%lu.%lu #%03lu",
      (unsigned long)(prep/10),(unsigned long)(prep%10),(unsigned long)(rgb/10),
      (unsigned long)(rgb%10),(unsigned long)(other/10),(unsigned long)(other%10),(unsigned long)sample);
}
inline void formatRenderWaitRow(char* out,size_t size,uint32_t waitUs,uint32_t fullUs,
    uint32_t barrierUs,uint32_t frames,uint32_t sample) {
  const auto wait=profileTenthsMs(waitUs,frames),full=profileTenthsMs(fullUs,frames),
      barrier=profileTenthsMs(barrierUs,frames);
  snprintf(out,size,"W%lu.%lu Q%lu.%lu B%lu.%lu ms/f #%03lu",
      (unsigned long)(wait/10),(unsigned long)(wait%10),(unsigned long)(full/10),
      (unsigned long)(full%10),(unsigned long)(barrier/10),(unsigned long)(barrier%10),(unsigned long)sample);
}
#endif
