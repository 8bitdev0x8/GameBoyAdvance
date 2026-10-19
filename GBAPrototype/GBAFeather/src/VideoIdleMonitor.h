#pragma once
#include <stdint.h>

// One idle sample can occur during DMA chaining. Require persistent idle before
// declaring the three-channel video chain stopped, including across timer wrap.
class VideoIdleMonitor {
  uint32_t idleSince_=0;
  bool watching_=false;
public:
  void reset() { watching_=false; }
  bool stopped(uint32_t now,bool anyBusy) {
    if(anyBusy) { reset();return false; }
    if(!watching_) { watching_=true;idleSince_=now;return false; }
    return uint32_t(now-idleSince_)>=20;
  }
};

// DMA can remain BUSY while waiting forever for its peripheral. Count completed
// IRQ handlers too, so this case is detected independently of the BUSY bits.
class VideoProgressMonitor {
  uint32_t lastCount_=0,lastChange_=0;
  bool initialized_=false;
public:
  void reset() { initialized_=false; }
  bool stalled(uint32_t now,uint32_t completed) {
    if(!initialized_ || completed!=lastCount_) {
      initialized_=true;lastCount_=completed;lastChange_=now;return false;
    }
    return uint32_t(now-lastChange_)>=100;
  }
};
