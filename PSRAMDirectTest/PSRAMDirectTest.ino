// Tests external memory before invoking the PSRAM heap allocator.
// A watchdog preserves the failing operation and displays it after reboot.
#include <Adafruit_dvhstx.h>
#include <hardware/watchdog.h>
#include <hardware/structs/watchdog.h>
#include <hardware/exception.h>
#include "SafeClock.h"

DVHSTX16 display(ADAFRUIT_FEATHER_RP2350_CFG,DVHSTX_RESOLUTION_320x240);
constexpr uint32_t MAGIC=0x50535235;
constexpr unsigned WORDS=2*1024*1024/4;
// Linker-reserved NOLOAD PSRAM: no pmalloc, no constructor, no startup memset.
PSRAM uint32_t reservedMemory[WORDS];
const char* stageName(uint32_t stage) {
  switch(stage) {
    case 1:return "1: DIRECT PSRAM READ";
    case 2:return "2: DIRECT PSRAM WRITE";
    case 3:return "3: 2 MiB PATTERN WRITE";
    case 4:return "4: 2 MiB PATTERN READ";
    case 5:return "5: PMALLOC AFTER DIRECT TEST";
    default:return "UNKNOWN";
  }
}
void line(int y,const char* s,uint16_t color=0xffff) {
  display.fillRect(0,y,320,14,0);display.setCursor(6,y);
  display.setTextColor(color);display.print(s);Serial.println(s);
}
static void __no_inline_not_in_flash_func(captureFault)() {
  watchdog_hw->scratch[2]=1; // Hardware fault; watchdog will restart the CPU.
  while(true)__asm volatile("nop");
}
void beginStage(uint32_t stage) {
  line(72,stageName(stage),0xffe0);
  delay(2000);
  watchdog_hw->scratch[0]=MAGIC;watchdog_hw->scratch[1]=stage;
  watchdog_hw->scratch[2]=0;
  watchdog_enable(1000,false);
}
void endStage() {
  watchdog_disable();watchdog_hw->scratch[0]=0;
}
void stopError(const char* error) {
  endStage();line(116,error,0xf800);
  while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(250);}
}
void setup() {
  const bool recovered=watchdog_caused_reboot() && watchdog_hw->scratch[0]==MAGIC;
  const uint32_t failedStage=watchdog_hw->scratch[1],fault=watchdog_hw->scratch[2];
  watchdog_disable();
  pinMode(LED_BUILTIN,OUTPUT);digitalWrite(LED_BUILTIN,HIGH);
  safeCpuAndRamClock();Serial.begin(115200);
  if(!display.begin())while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(100);}
  display.fillScreen(0);display.setTextWrap(false);
  line(8,"PSRAM DIRECT / ALLOCATOR TEST v5",0x07ff);
  line(28,"CPU 120 MHz / PSRAM clock 30 MHz");
  if(recovered) {
    line(60,"PREVIOUS TEST CRASHED AT:",0xf800);
    line(84,stageName(failedStage),0xffe0);
    line(108,fault?"CPU HARDFAULT CAPTURED":"WATCHDOG TIMEOUT / STALLED ACCESS");
    line(144,"Report the stage shown above.");
    // Leave the result visible; do not touch PSRAM or retry automatically.
    watchdog_hw->scratch[0]=0;
    while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(500);}
  }
  exception_set_exclusive_handler(HARDFAULT_EXCEPTION,captureFault);
  char info[70];snprintf(info,sizeof(info),"Reserved buffer: %08lX / 2 MiB",
    (unsigned long)(uintptr_t)reservedMemory);line(48,info);
  volatile uint32_t* direct=reservedMemory;

  beginStage(1);
  const uint32_t initial=direct[0];
  endStage();snprintf(info,sizeof(info),"READ OK: %08lX",(unsigned long)initial);line(94,info,0x07e0);

  beginStage(2);
  direct[0]=0x12345678;direct[1]=0xa5a55a5a;
  if(direct[0]!=0x12345678 || direct[1]!=0xa5a55a5a)stopError("DIRECT WRITE/READ MISMATCH");
  endStage();line(94,"DIRECT WRITE/READ OK",0x07e0);

  beginStage(3);
  for(unsigned base=0;base<WORDS;base+=1024) {
    watchdog_update();
    for(unsigned i=base;i<base+1024;++i)direct[i]=(i*0x9e3779b9u)^0x5aa5a55a;
    delay(1);
  }
  endStage();line(94,"2 MiB WRITE OK",0x07e0);

  beginStage(4);
  for(unsigned base=0;base<WORDS;base+=1024) {
    watchdog_update();
    for(unsigned i=base;i<base+1024;++i)
      if(direct[i]!=((i*0x9e3779b9u)^0x5aa5a55a))stopError("2 MiB READ MISMATCH");
    delay(1);
  }
  endStage();line(94,"2 MiB DIRECT TEST PASSED",0x07e0);

  beginStage(5);
  void* allocated=pmalloc(4096); // Separate remaining heap, outside reserved array.
  endStage();
  if(!allocated)stopError("PMALLOC RETURNED NULL");
  snprintf(info,sizeof(info),"PMALLOC OK: %08lX",(unsigned long)(uintptr_t)allocated);
  line(116,info,0x07e0);
  line(144,"PASS: DIRECT PSRAM + ALLOCATOR",0x07e0);
}
void loop() {
  digitalWrite(LED_BUILTIN,(millis()/500)&1);delay(10);
}
