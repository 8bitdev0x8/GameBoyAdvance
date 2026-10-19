#pragma once
#include <array>
#include <stdint.h>

// Preserve a short UART button edge for one complete guest frame. At low
// emulation speed a press and release can otherwise fall between game polls.
class FrameInputLatch {
  uint16_t physical_=0,pending_=0;
  std::array<uint32_t,10> pressedAt_{};
public:
  void clear() { physical_=pending_=0; }
  void update(uint16_t keys,uint32_t frame) {
    keys&=0x03ffu;
    const uint16_t rising=keys&~physical_;
    for(unsigned bit=0;bit<10;++bit)
      if(rising&(1u<<bit))pressedAt_[bit]=frame;
    pending_|=rising;physical_=keys;
  }
  uint16_t value(uint32_t frame) {
    for(unsigned bit=0;bit<10;++bit)
      if((pending_&(1u<<bit)) && uint32_t(frame-pressedAt_[bit])>=2)
        pending_&=uint16_t(~(1u<<bit));
    return physical_|pending_;
  }
};
