#ifdef GBA_ENABLE_PC_PROFILE
#include <Arduino.h>
#include "../PcProfiler.h"
#include "PcProfilerFormat.h"
#include <hardware/irq.h>
#include <hardware/timer.h>
#include <hardware/sync.h>
#include <cstring>

namespace {
volatile uint16_t hits[GBA_PC_RANGE_CAPACITY]{};
volatile uint32_t samples=0,unmapped=0,invalid=0,irqSamples=0,handlerUs=0;
uint32_t randomState=0x61c88647u,lastSnapshot=0;
int alarmNumber=-1;
unsigned samplerIrq=0;
bool attempted=false;
GbaPcProfileSnapshot snapshot;
}

extern "C" void __no_inline_not_in_flash_func(gba_pc_profile_capture)(const uint32_t* frame,uint32_t excReturn) {
  auto* timer=PICO_DEFAULT_TIMER_INSTANCE();
  const uint32_t started=timer->timerawl;
  timer->intr=1u<<alarmNumber;
  uint32_t pc=0;
  samples=samples+1;
  if(gbaProfileFramePc(frame,excReturn,0x20000000u,0x20080000u,pc)) {
    if(!(excReturn&8u))irqSamples=irqSamples+1;
    const int index=gbaProfileFindRange(pc,gbaPcRanges,gbaPcRangeCount);
    if(index>=0) {
      const uint16_t old=hits[index];
      if(old!=0xffffu)hits[index]=old+1;
    } else unmapped=unmapped+1;
  } else invalid=invalid+1;
  // Dither the period to avoid repeatedly sampling the same video/game phase.
  randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;
  const uint32_t finished=timer->timerawl;
  handlerUs=handlerUs+uint32_t(finished-started);
  timer->alarm[alarmNumber]=finished+1933u+(randomState&255u);
}

// Capture the original exception SP before a C prologue changes MSP.
extern "C" __attribute__((naked,used,section(".time_critical.gba_pc_profile_irq"))) void gba_pc_profile_irq() {
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "ldr r2, =gba_pc_profile_capture\n"
    "bx r2\n");
}

void gbaPcProfileStart() {
  if(attempted)return;
  attempted=true;
  if(!gbaPcRangeCount || gbaPcRangeCount>GBA_PC_RANGE_CAPACITY)return;
  alarmNumber=hardware_alarm_claim_unused(false);
  if(alarmNumber<0)return;
  auto* timer=PICO_DEFAULT_TIMER_INSTANCE();
  samplerIrq=TIMER_ALARM_IRQ_NUM(timer,unsigned(alarmNumber));
  irq_set_enabled(samplerIrq,false);
  irq_set_exclusive_handler(samplerIrq,gba_pc_profile_irq);
  // HDMI retains its highest priority. This sampler never preempts scanout.
  irq_set_priority(samplerIrq,PICO_DEFAULT_IRQ_PRIORITY);
  timer->intr=1u<<alarmNumber;
  hw_set_bits(&timer->inte,1u<<alarmNumber);
  timer->alarm[alarmNumber]=timer->timerawl+2059u;
  lastSnapshot=millis();snapshot.active=true;
  irq_set_enabled(samplerIrq,true);
}

void gbaPcProfileUpdate() {
  if(alarmNumber<0)return;
  const uint32_t now=millis();
  if(uint32_t(now-lastSnapshot)<5000u)return;
  // Mask this alarm only; HDMI and all other interrupts continue running.
  irq_set_enabled(samplerIrq,false);
  snapshot.samples=samples;snapshot.unmapped=unmapped;snapshot.invalid=invalid;
  snapshot.irqSamples=irqSamples;snapshot.handlerUs=handlerUs;snapshot.elapsedMs=now-lastSnapshot;
  samples=0;unmapped=0;invalid=0;irqSamples=0;handlerUs=0;
  uint32_t best[3]{};unsigned indices[3]{};
  for(unsigned index=0;index<gbaPcRangeCount;++index) {
    const uint32_t count=hits[index];hits[index]=0;
    for(unsigned rank=0;rank<3;++rank)if(count>best[rank]) {
      for(unsigned move=2;move>rank;--move){best[move]=best[move-1];indices[move]=indices[move-1];}
      best[rank]=count;indices[rank]=index;break;
    }
  }
  irq_set_enabled(samplerIrq,true);
  for(unsigned rank=0;rank<3;++rank) {
    snapshot.rangeIndex[rank]=best[rank]?uint16_t(indices[rank]):uint16_t(0xffffu);
    std::memcpy(snapshot.label[rank],best[rank]?gbaPcRanges[indices[rank]].label:"none",best[rank]?24:5);
    snapshot.label[rank][23]=0;
    snapshot.percent[rank]=snapshot.samples?best[rank]*100u/snapshot.samples:0;
  }
  lastSnapshot=now;
}

const GbaPcProfileSnapshot& gbaPcProfileSnapshot(){return snapshot;}
#endif
