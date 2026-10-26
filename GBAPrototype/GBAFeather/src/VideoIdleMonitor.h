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

// A busy DMA chain may still be sending malformed HSTX commands. At this
// project's fixed 640x480 timing, one IRQ per line gives about 31.5 kHz.
class VideoRateMonitor {
  uint32_t lastTime_=0,lastCount_=0;
  unsigned badWindows_=0;
  bool initialized_=false;
  uint32_t minRate_=25000,maxRate_=38000;
public:
  VideoRateMonitor()=default;
  VideoRateMonitor(uint32_t minRate,uint32_t maxRate):minRate_(minRate),maxRate_(maxRate){}
  void reset() { initialized_=false;badWindows_=0; }
  bool invalid(uint32_t now,uint32_t completed) {
    if(!initialized_) {
      initialized_=true;lastTime_=now;lastCount_=completed;return false;
    }
    const uint32_t elapsed=now-lastTime_;
    if(elapsed<500)return false;
    const uint32_t count=completed-lastCount_;
    lastTime_=now;lastCount_=completed;
    // Ignore long startup pauses and require two consecutive bad windows.
    if(elapsed>2500){badWindows_=0;return false;}
    const uint64_t scaled=uint64_t(count)*1000;
    if(scaled>=uint64_t(elapsed)*minRate_ && scaled<=uint64_t(elapsed)*maxRate_) {
      badWindows_=0;return false;
    }
    return ++badWindows_>=2;
  }
};
