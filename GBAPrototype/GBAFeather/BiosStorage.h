#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

// Startup-only selection. Both the embedded source and an optional successful
// allocation must outlive the emulator. The board keeps them until reset.
struct BootBiosStorage {
  std::span<const uint8_t> bytes;
  bool sramCopy=false;
};
inline constexpr size_t BIOS_HEAP_RESERVE_BYTES=8*1024;
BootBiosStorage prepareBootBios(std::span<const uint8_t> source,
    size_t availableHeap,void* (*allocate)(size_t));
