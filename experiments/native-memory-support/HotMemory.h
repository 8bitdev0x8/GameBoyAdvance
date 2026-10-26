#pragma once
#include <stddef.h>
#include <stdint.h>
// This backend extends the SAME registered allocator used by Rust Vec. It never
// routes ordinary allocations by size. Allocation is by an explicit region ID.
// Single core0 executor; no allocation/free from IRQ/core1.
enum : uint32_t { GBA_HOT_IWRAM=1,GBA_HOT_VRAM=2 };
enum : uint32_t { GBA_HOT_NOT_OWNED=0,GBA_HOT_OK=1,GBA_HOT_INVALID=2 };
struct GbaHotMemoryStats {uint32_t requests,allocations,frees,exhausted,bad_region,bad_layout,bad_pointer,bad_guard,realloc_rejected;};
extern "C" {
void* gba_board_alloc_region(uint32_t region,size_t bytes,size_t align);
// Ordinary GlobalAlloc free MUST call this before its general allocator. It
// checks exact pointer, lease, size, alignment, and both32-byte guards.
uint32_t gba_board_region_try_free(void* pointer,size_t bytes,size_t align);
// 0 ordinary allocation;1 named region: refuse realloc, leave it intact;
// 2 invalid interior/layout/lease/guard: fatal allocator contract violation.
uint32_t gba_board_region_realloc_probe(void*,size_t bytes,size_t align,size_t new_bytes);
uint32_t gba_board_region_validate(uint32_t region);
GbaHotMemoryStats gba_board_region_stats(void);
}
