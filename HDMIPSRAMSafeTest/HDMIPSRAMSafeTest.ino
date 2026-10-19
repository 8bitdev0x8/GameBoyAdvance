// Conservative staged test: HSTX video and PSRAM, no ROM/emulator.
#include <Adafruit_dvhstx.h>
#include <hardware/clocks.h>
#include <hardware/structs/qmi.h>
#include <hardware/regs/qmi.h>
#include <hardware/sync.h>

DVHSTX16 display(ADAFRUIT_FEATHER_RP2350_CFG,DVHSTX_RESOLUTION_320x240);
volatile uint32_t* memory=nullptr;
uint32_t passes=0;
constexpr unsigned WORDS=2*1024*1024/4;

// The installed HSTX library uses PLL_USB=480 MHz for clk_sys and PLL_SYS
// separately for video. Divide only clk_sys: do not retune the video PLL.
static void __no_inline_not_in_flash_func(safeCpuAndRamClock)() {
  uint32_t irq=save_and_disable_interrupts();
  clock_configure(clk_sys,CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,480000000,120000000);
  // 120 MHz CPU / 4 = 30 MHz PSRAM. 4 half-cycle receive delay,
  // <= 8 us select, >= 18 ns deselect including divided-clock contribution.
  qmi_hw->m[1].timing=(1u<<QMI_M1_TIMING_COOLDOWN_LSB)|
    (QMI_M1_TIMING_PAGEBREAK_VALUE_1024<<QMI_M1_TIMING_PAGEBREAK_LSB)|
    (15u<<QMI_M1_TIMING_MAX_SELECT_LSB)|
    (1u<<QMI_M1_TIMING_MIN_DESELECT_LSB)|
    (4u<<QMI_M1_TIMING_RXDELAY_LSB)|(4u<<QMI_M1_TIMING_CLKDIV_LSB);
  __dmb();restore_interrupts(irq);
}
void line(int y,const char* s,uint16_t color=0xffff) {
  display.fillRect(0,y,320,14,0);display.setCursor(8,y);
  display.setTextColor(color);display.print(s);Serial.println(s);
}
void stage(const char* s,unsigned led) {
  line(76,s,0xffe0);digitalWrite(LED_BUILTIN,led);
  // The stage must be visibly held before a potentially failing operation.
  delay(3000);
}
void setup() {
  pinMode(LED_BUILTIN,OUTPUT);digitalWrite(LED_BUILTIN,HIGH);
  safeCpuAndRamClock();Serial.begin(115200);
  if(!display.begin())while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(100);}
  display.fillScreen(0);display.setTextWrap(false);
  line(8,"HDMI / PSRAM SAFE TEST v4",0x07ff);
  char info[70];snprintf(info,sizeof(info),"CPU %lu MHz / PSRAM %lu MiB",
    (unsigned long)(clock_get_hz(clk_sys)/1000000),
    (unsigned long)(rp2040.getPSRAMSize()/(1024*1024)));
  line(28,info);
  line(48,"No game. Separate PLL retained for video.");
  stage("STAGE 0: VIDEO ONLY",HIGH);
  stage("STAGE 1: ABOUT TO ALLOCATE 2 MiB",LOW);
  memory=static_cast<volatile uint32_t*>(pmalloc(WORDS*4));
  if(!memory){line(100,"ALLOCATION FAILED",0xf800);while(true)delay(100);}
  snprintf(info,sizeof(info),"Allocated address: %08lX",(unsigned long)(uintptr_t)memory);
  line(100,info,0x07e0);delay(3000);
}
void loop() {
  const uint32_t salt=0x9e3779b9u*(passes+1);
  stage("STAGE 2: WRITING PSRAM",HIGH);
  for(unsigned base=0;base<WORDS;base+=1024) {
    for(unsigned i=base;i<base+1024;++i)memory[i]=(i*0x01010101u)^salt;
    delay(1); // Let video interrupts run between small chunks.
  }
  __dmb();
  stage("STAGE 3: READING PSRAM",LOW);
  // Ordinary mapped reads, avoiding an additional alias-address variable.
  // Full sequential passes exceed the XIP cache capacity many times over.
  for(unsigned base=0;base<WORDS;base+=1024) {
    for(unsigned i=base;i<base+1024;++i) {
      const uint32_t actual=memory[i],expected=(i*0x01010101u)^salt;
      if(actual!=expected) {
        char info[80];snprintf(info,sizeof(info),"FAIL word %lu got %08lX",(unsigned long)i,(unsigned long)actual);
        line(124,info,0xf800);while(true)delay(100);
      }
    }
    delay(1);
  }
  ++passes;char info[70];snprintf(info,sizeof(info),"PASS %lu - 2 MiB write/read verified",(unsigned long)passes);
  line(124,info,0x07e0);
  display.fillRect(8,160,304,48,passes&1?0x001f:0xf800);
  line(218,"If video stops, report the last STAGE.");
  delay(3000);
}
