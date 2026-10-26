#include "HotMemory.h"
#include <string.h>
namespace {
template<size_t N>struct alignas(32) Guarded {uint8_t before[32];alignas(32)uint8_t data[N];uint8_t after[32];};
Guarded<32768> iwram;
Guarded<98304> vram;
struct Lease {uint32_t active,generation;};
Lease leases[2]{};
GbaHotMemoryStats stats{};
struct Region {uint8_t *base,*before,*after;size_t bytes;Lease* lease;};
bool region(uint32_t id,Region& r){if(id==GBA_HOT_IWRAM){r={iwram.data,iwram.before,iwram.after,sizeof(iwram.data),&leases[0]};return true;}if(id==GBA_HOT_VRAM){r={vram.data,vram.before,vram.after,sizeof(vram.data),&leases[1]};return true;}return false;}
bool guards(const Region& r){for(unsigned i=0;i<32;++i)if(r.before[i]!=0xd3u||r.after[i]!=0x6au)return false;return true;}
uint32_t owner(void* p,Region& r){const uintptr_t pointer=reinterpret_cast<uintptr_t>(p);for(uint32_t id=1;id<=2;++id){region(id,r);const uintptr_t begin=reinterpret_cast<uintptr_t>(r.before),end=reinterpret_cast<uintptr_t>(r.after)+32;if(pointer>=begin&&pointer<end)return id;}return 0;}
uint32_t validate(void* p,size_t bytes,size_t align,Region& r){if(!owner(p,r))return GBA_HOT_NOT_OWNED;if(p!=r.base||!r.lease->active){++stats.bad_pointer;return GBA_HOT_INVALID;}if(bytes!=r.bytes||align!=1){++stats.bad_layout;return GBA_HOT_INVALID;}if(!guards(r)){++stats.bad_guard;return GBA_HOT_INVALID;}return GBA_HOT_OK;}
}
extern "C" void* gba_board_alloc_region(uint32_t id,size_t bytes,size_t align){++stats.requests;Region r{};if(!region(id,r)){++stats.bad_region;return nullptr;}if(bytes!=r.bytes||align!=1){++stats.bad_layout;return nullptr;}if(r.lease->active){++stats.exhausted;return nullptr;}memset(r.before,0xd3,32);memset(r.after,0x6a,32);r.lease->active=1;++r.lease->generation;++stats.allocations;return r.base;}
extern "C" uint32_t gba_board_region_try_free(void* p,size_t bytes,size_t align){Region r{};const auto result=validate(p,bytes,align,r);if(result!=GBA_HOT_OK)return result;r.lease->active=0;++stats.frees;return GBA_HOT_OK;}
extern "C" uint32_t gba_board_region_realloc_probe(void* p,size_t bytes,size_t align,size_t){Region r{};const auto result=validate(p,bytes,align,r);if(result==GBA_HOT_OK)++stats.realloc_rejected;return result;}
extern "C" uint32_t gba_board_region_validate(uint32_t id){Region r{};if(!region(id,r))return GBA_HOT_INVALID;return r.lease->active&&guards(r)?GBA_HOT_OK:GBA_HOT_INVALID;}
extern "C" GbaHotMemoryStats gba_board_region_stats(){return stats;}
