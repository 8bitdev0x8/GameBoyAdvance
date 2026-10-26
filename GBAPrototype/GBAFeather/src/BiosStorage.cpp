#include "../BiosStorage.h"
#include <cstring>

// Keep the copy routine separate from the embedded array's translation unit;
// GCC otherwise emits both an addressable array and a second constant copy.
BootBiosStorage prepareBootBios(std::span<const uint8_t> source,
    size_t availableHeap,void* (*allocate)(size_t)) {
  if(source.empty() || !allocate || source.size()>availableHeap ||
      availableHeap-source.size()<BIOS_HEAP_RESERVE_BYTES)return {source,false};
  auto* copy=static_cast<uint8_t*>(allocate(source.size()));
  if(!copy)return {source,false};
  std::memcpy(copy,source.data(),source.size());
  return {{copy,source.size()},true};
}
