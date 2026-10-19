// Isolate video + PSRAM startup from all ROM/emulator code.
#include <Adafruit_dvhstx.h>
#include "BoardStartup.h"
DVHSTX16 display(ADAFRUIT_FEATHER_RP2350_CFG,DVHSTX_RESOLUTION_320x240);
uint32_t* memory=nullptr;
uint32_t passes=0;
void line(int y,const char* s,uint16_t color=0xffff) {
  display.fillRect(0,y,320,12,0);display.setCursor(8,y);
  display.setTextColor(color);display.print(s);
  Serial.println(s);
}
void setup() {
  pinMode(LED_BUILTIN,OUTPUT);digitalWrite(LED_BUILTIN,HIGH);
  configureVideoPsram();
  Serial.begin(115200);
  if(!display.begin())while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(100);}
  display.setTextWrap(false);display.fillScreen(0);
  line(12,"HDMI + PSRAM DIAGNOSTIC v2",0x07ff);
  line(32,"HDMI started. PSRAM test starts in 3s.");
  char info[60];snprintf(info,sizeof(info),"CPU %lu MHz PSRAM %lu MiB",
    (unsigned long)(clock_get_hz(clk_sys)/1000000),
    (unsigned long)(rp2040.getPSRAMSize()/(1024*1024)));
  line(52,info);delay(3000);
  if(rp2040.getPSRAMSize()<8u*1024u*1024u) {
    line(80,"ERROR: 8 MiB PSRAM not detected",0xf800);while(true)delay(100);
  }
  memory=static_cast<uint32_t*>(pmalloc(2*1024*1024));
  if(!memory){line(80,"ERROR: PSRAM allocation failed",0xf800);while(true)delay(100);}
}
void loop() {
  constexpr unsigned WORDS=2*1024*1024/4;
  const uint32_t salt=0x9e3779b9u*(passes+1);
  line(80,"Testing 2 MiB PSRAM...");
  for(unsigned i=0;i<WORDS;++i)memory[i]=(i*0x01010101u)^salt;
  __dmb();
  // Force reads through the uncached PSRAM alias; avoid an XIP-cache-only test.
  volatile uint32_t* uncached=reinterpret_cast<volatile uint32_t*>(
    reinterpret_cast<uintptr_t>(memory)+0x04000000u);
  for(unsigned i=0;i<WORDS;++i)if(uncached[i]!=((i*0x01010101u)^salt)) {
    line(104,"FAIL: PSRAM data mismatch",0xf800);while(true)delay(100);
  }
  ++passes;char info[70];snprintf(info,sizeof(info),"PASS %lu - video + PSRAM working",(unsigned long)passes);
  line(104,info,0x07e0);
  display.fillRect(8,140,304,60,passes&1?0x001f:0xf800);
  digitalWrite(LED_BUILTIN,passes&1);delay(1000);
}
