// Experimental GBA interpreter + compressed ROM cache + HDMI + PS5 UART.
// Build using ../build.ps1: the core requires C++20 and embedded build flags.
#include <Arduino.h>
#include <Adafruit_dvhstx.h>
#include "src/gba/core/emulator.hpp"
#include "CompressedRom.h"
#include "ControllerState.h"
#include "UartProtocol.h"
#include "src/ReplacementBios.h"
#include "BoardStartup.h"
#include "MemoryBenchmark.h"
#include <hardware/watchdog.h>
#include <hardware/structs/watchdog.h>
#include <hardware/exception.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <new>
#include "src/BootTrace.h"
#include "src/VideoIdleMonitor.h"
#include "src/FrameInputLatch.h"
#include "src/gba/core/render_worker.hpp"
#include <pico/multicore.h>
extern "C" uint8_t __StackBottom, __StackTop;
void textLine(int y,const char* message,uint16_t color=0xffff);
uint16_t gbaKeys();

constexpr uint32_t BOOT_MAGIC=0x47424128;
const char* bootStageName(uint32_t stage) {
  switch(stage) {
    case 1:return "1: VERIFY ROM BLOCKS";
    case 2:return "2: CONSTRUCT EMULATOR";
    case 3:return "3: SCAN CARTRIDGE";
    case 4:return "4: CONFIGURE SAVE MEMORY";
    case 5:return "5: LOAD REPLACEMENT BIOS";
    case 6:return "6: RESET GBA CPU";
    case 7:return "7: RUN CPU / WAIT FIRST FRAME";
    case 8:return "8: PRESENT FIRST FRAME";
    case 9:return "9: RUN GAME";
    case 10:return "10: RESTART HDMI OUTPUT";
    case 11:return "11: START CORE 1 RENDERER";
    case 12:return "12: VERIFY LARGE CACHE RETENTION";
    case 13:return "13: MEASURE MEMORY ACCESS";
    default:return gbaBootStepName(stage);
  }
}
static void __no_inline_not_in_flash_func(captureGbaFault)() {
  watchdog_hw->scratch[2]=1;
  watchdog_hw->scratch[3]=*(volatile uint32_t*)0xe000ed28; // Cortex-M CFSR.
  while(true)__asm volatile("nop");
}

DVHSTX16 display(ADAFRUIT_FEATHER_RP2350_CFG,DVHSTX_RESOLUTION_320x240);
gba::Emulator* emulator=nullptr;
gba::ScanlineWorker renderWorker;
std::atomic<bool> renderCoreReady{false};
alignas(16) uint32_t renderStack[2048+16]; // 8 KiB stack plus a 64-byte canary.
constexpr uint32_t RENDER_CANARY=0x71a6c35d;
static void __no_inline_not_in_flash_func(renderCoreMain)() {
  renderCoreReady.store(true,std::memory_order_release);
  __sev();
  while(true)if(!renderWorker.run_one())__wfe();
}
bool renderStackOK() {
  for(unsigned i=0;i<16;++i)if(renderStack[i]!=RENDER_CANARY)return false;
  return true;
}
CompressedRom* rom=nullptr;
ControllerState controller;
FrameInputLatch inputLatch;
PadWire::Parser uartParser;
uint32_t lastPacket=0;
bool havePacket=false;
uint32_t emulatedFrames=0,fps=0,lastFps=0,fpsFrames=0;
uint32_t lastProfileMisses=0,lastProfileInflate=0;
uint64_t lastProfileVblankCycles=0,lastProfileGuestCycles=0;
uint32_t runWallUs=0,copyWallUs=0,lastRunWallUs=0,lastCopyWallUs=0,lastRenderWallUs=0;
uint32_t lastWaitUs=0;
constexpr size_t VIDEO_BYTES=320*240*sizeof(uint16_t);
uint8_t* videoBackup=nullptr;
bool videoReady=false,repairingVideo=false;
bool directVideoReady=false;
uint32_t videoRestarts=0;
uint32_t lastVideoRestart=0;
VideoIdleMonitor videoIdle;
VideoProgressMonitor videoProgress;
volatile uint32_t videoIrqCount=0;
volatile uint32_t videoIrqWallUs=0;
MemoryBenchmarkResult memoryResults[3]{};
irq_handler_t videoDriverIrq=nullptr;
static void __no_inline_not_in_flash_func(countVideoIrq)() {
  const uint32_t started=time_us_32();
  videoDriverIrq();
  videoIrqWallUs=videoIrqWallUs+uint32_t(time_us_32()-started);
  videoIrqCount=videoIrqCount+1;
}
void installVideoProbe() {
  irq_set_enabled(DMA_IRQ_2,false);
  const irq_handler_t current=irq_get_exclusive_handler(DMA_IRQ_2);
  if(current!=countVideoIrq) {
    videoDriverIrq=current;
    irq_remove_handler(DMA_IRQ_2,current);
    irq_set_exclusive_handler(DMA_IRQ_2,countVideoIrq);
  }
  irq_set_enabled(DMA_IRQ_2,true);
  videoProgress.reset();
}

void recoveryLed() {
  const uint32_t t=millis()%2000;
  digitalWrite(LED_BUILTIN,t<400 && (t%200)<70);
}
void restartVideo(bool force) {
  if(!videoReady || !videoBackup || repairingVideo)return;
  const uint32_t now=millis();
  // Allow the monitor to relock and avoid repeated allocations every poll.
  if(!force && videoRestarts && uint32_t(now-lastVideoRestart)<1000)return;
  lastVideoRestart=now;
  repairingVideo=true;
  const uint32_t previousStage=watchdog_hw->scratch[1];
  watchdog_hw->scratch[1]=10;
  watchdog_update();
  // Core 1 must finish before the display allocation is freed. Core 0 is the
  // sole producer, so no new render job can be submitted during this restart.
  if(emulator)emulator->flush_render();
  if(directVideoReady)renderWorker.set_video_target(nullptr,0);
  memcpy(videoBackup,display.getBuffer(),VIDEO_BYTES);
  display.end();
  if(!display.begin()) {
    // The old buffer was freed; do not draw through it after a failed begin.
    videoReady=false;
    while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));busy_wait_ms(100);}
  }
  installVideoProbe();
  memcpy(display.getBuffer(),videoBackup,VIDEO_BYTES);
  if(directVideoReady)renderWorker.set_video_target(display.getBuffer()+48*320+40,320);
  ++videoRestarts;
  videoIdle.reset();
  watchdog_hw->scratch[1]=previousStage;
  watchdog_update();
  repairingVideo=false;
  display.fillRect(0,34,320,12,0);
  display.setTextColor(0x07e0);display.setCursor(4,36);
  display.print("HDMI output reset: ");display.print(videoRestarts);
}
void serviceVideo() {
  if(!videoReady || !videoBackup || repairingVideo)return;
  static uint32_t lastCheck=0;
  const uint32_t now=millis();
  if(uint32_t(now-lastCheck)<10)return;
  lastCheck=now;
  // The installed HSTX driver claims channels 0..2 and DMA_IRQ_2 exclusively.
  const bool busy=dma_channel_is_busy(0) || dma_channel_is_busy(1) || dma_channel_is_busy(2);
  const bool idle=videoIdle.stopped(now,busy);
  const bool stalled=videoProgress.stalled(now,videoIrqCount);
  if(idle || stalled)restartVideo(false);
}
void runtimeProgress() {
  static uint32_t lastTime=0,lastIrqs=0,lastIrqUs=0;
  const uint32_t now=millis();
  if(uint32_t(now-lastTime)<1000)return;
  const uint32_t count=videoIrqCount;
  const uint32_t irqUs=videoIrqWallUs;
  const uint32_t irqMsPerSecond=uint32_t(irqUs-lastIrqUs)/uint32_t(now-lastTime);
  const uint32_t irqRate=uint64_t(uint32_t(count-lastIrqs))*1000/uint32_t(now-lastTime);
  lastTime=now;lastIrqs=count;lastIrqUs=irqUs;
  char line[70];
  snprintf(line,sizeof(line),"HDMI reset %lu IRQ/s %lu %lu ms/s",(unsigned long)videoRestarts,(unsigned long)irqRate,(unsigned long)irqMsPerSecond);
  textLine(34,line,0x07e0);
  if(!emulatedFrames) {
    textLine(4,"GBA CPU running / waiting for first frame",0x07ff);
    snprintf(line,sizeof(line),"PC %08lX cycles %lu",(unsigned long)emulator->cpu().state().regs[15],
      (unsigned long)emulator->cpu().current_cycle());
    textLine(18,line);
  }
}

void textLine(int y,const char* message,uint16_t color) {
  serviceVideo();
  display.fillRect(0,y,320,12,0);
  display.setTextSize(1);display.setTextColor(color);display.setCursor(4,y+2);
  display.print(message);
}
void fail(const char* message) {
  watchdog_disable();watchdog_hw->scratch[0]=0;
  textLine(100,message,0xf800);
  textLine(116,"Stop. Reset board after fixing this.");
  while(true) {digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(200);}
}
void beginBootStage(uint32_t stage) {
  // Arm recovery before drawing, waiting, or invoking cartridge code.
  watchdog_hw->scratch[0]=BOOT_MAGIC;watchdog_hw->scratch[1]=stage;
  watchdog_hw->scratch[2]=0;watchdog_hw->scratch[3]=0;
  watchdog_enable(8000,false);
  textLine(80,bootStageName(stage),0xffe0);
  busy_wait_ms(1000);
  watchdog_update();
}
void cartridgeScanProgress(size_t completed,size_t total) {
  watchdog_update();
  pollController();
  if(completed==total || (completed%(256*1024))==0) {
    char line[64];
    snprintf(line,sizeof(line),"Scan %lu / %lu KiB",(unsigned long)(completed/1024),(unsigned long)(total/1024));
    textLine(108,line,0x07e0);
  }
}
void constructorProgress(uint32_t step) {
  watchdog_hw->scratch[1]=step;
  textLine(96,gbaBootStepName(step),0xffe0);
  // Keep constructor instrumentation out of USB's task/mutex machinery.
  uint32_t sp;asm volatile("mrs %0, msp":"=r"(sp));
  char info[64];
  snprintf(info,sizeof(info),"Arena %lu KiB /7680  Stack %lu",(unsigned long)(gbaPsramUsed()/1024),
    (unsigned long)(sp-reinterpret_cast<uintptr_t>(&__StackBottom)));
  textLine(112,info);
  watchdog_update();
  busy_wait_ms(300);
}
void pollController() {
  serviceVideo();
  // Wall-clock heartbeat distinguishes lost video from stopped application work.
  digitalWrite(LED_BUILTIN,(millis()/500)&1);
  uint8_t p[PadWire::PACKET_SIZE];
  for(unsigned budget=0;budget<1024 && Serial1.available();++budget) {
    if(uartParser.push(uint8_t(Serial1.read()),p)) {
      lastPacket=millis();havePacket=true;
      if(p[1]) {
        memcpy(controller.raw,p+4,78);controller.connected=true;
        inputLatch.update(gbaKeys(),emulatedFrames);
      } else {controller.clear();inputLatch.clear();}
    }
  }
  if(controller.connected && (!havePacket || uint32_t(millis()-lastPacket)>=250)) {
    controller.clear();inputLatch.clear();
  }
}
uint16_t gbaKeys() {
  if(!controller.connected)return 0;
  uint16_t keys=0;
  if(controller.button(ControllerState::CROSS))keys|=1u<<0;
  if(controller.button(ControllerState::CIRCLE))keys|=1u<<1;
  if(controller.button(ControllerState::CREATE))keys|=1u<<2;
  if(controller.button(ControllerState::OPTIONS))keys|=1u<<3;
  if(controller.right() || controller.leftX()>50)keys|=1u<<4;
  if(controller.left() || controller.leftX()<-50)keys|=1u<<5;
  if(controller.up() || controller.leftY()<-50)keys|=1u<<6;
  if(controller.down() || controller.leftY()>50)keys|=1u<<7;
  if(controller.button(ControllerState::R1))keys|=1u<<8;
  if(controller.button(ControllerState::L1))keys|=1u<<9;
  return keys;
}
void presentGame() {
  const uint32_t copyStarted=micros();
  // consume_frame_ready() already drained core 1. Each dirty scanline has
  // been converted and written into the HDMI buffer; no frame copy remains.
  copyWallUs+=uint32_t(micros()-copyStarted);
  ++emulatedFrames;++fpsFrames;
  const uint32_t now=millis();
  // Redraw diagnostics once per second; game pixels still update every frame.
  if(uint32_t(now-lastFps)<1000)return;
  const uint32_t elapsed=now-lastFps;
  fps=uint64_t(fpsFrames)*10000/elapsed;
  const uint64_t guestCycles=emulator->cpu().current_cycle();
  const uint64_t vblankCycles=emulator->cpu().vblank_skipped_cycles();
  const uint32_t vblankPercent=guestCycles>lastProfileGuestCycles?
    uint32_t((vblankCycles-lastProfileVblankCycles)*100/(guestCycles-lastProfileGuestCycles)):0;
  lastProfileGuestCycles=guestCycles;lastProfileVblankCycles=vblankCycles;
  const uint32_t rendered=emulator->render_wall_us();
  const uint32_t runMs=uint32_t(runWallUs-lastRunWallUs)/(fpsFrames*1000u);
  const uint32_t drawMs=uint32_t(rendered-lastRenderWallUs)/(fpsFrames*1000u);
  const uint32_t copyMs=uint32_t(copyWallUs-lastCopyWallUs)/(fpsFrames*1000u);
  const uint32_t waited=renderWorker.wait_wall_us();
  const uint32_t waitMs=uint32_t(waited-lastWaitUs)/(fpsFrames*1000u);
  lastWaitUs=waited;
  lastRunWallUs=runWallUs;lastCopyWallUs=copyWallUs;lastRenderWallUs=rendered;
  fpsFrames=0;lastFps=now;
  char line[80];
  snprintf(line,sizeof(line),"v28 %lu.%lu FPS | PAD %s | VBskip %lu%%",(unsigned long)(fps/10),(unsigned long)(fps%10),controller.connected?"ON":"OFF",(unsigned long)vblankPercent);
  textLine(4,line,0x07ff);
  snprintf(line,sizeof(line),"Run %lu Draw %lu Copy %lu Wait %lu ms/f",(unsigned long)runMs,(unsigned long)drawMs,(unsigned long)copyMs,(unsigned long)waitMs);
  textLine(18,line);
  const uint32_t missesPerSecond=uint64_t(rom->misses-lastProfileMisses)*1000/elapsed;
  const uint32_t inflateMsPerSecond=uint32_t(rom->inflateUs-lastProfileInflate)/elapsed;
  lastProfileMisses=rom->misses;lastProfileInflate=rom->inflateUs;
  snprintf(line,sizeof(line),"ROM miss/s %lu decode %lu ms/s",(unsigned long)missesPerSecond,(unsigned long)inflateMsPerSecond);
  textLine(216,line);
  snprintf(line,sizeof(line),"PC %08lX key %03X reads %lu",
    (unsigned long)emulator->cpu().state().regs[15],unsigned((~emulator->bus().keyinput())&0x03ffu),
    (unsigned long)emulator->bus().keyinput_read_count());
  textLine(228,line,0x8410);
  if((now/6000u)%2u) {
    snprintf(line,sizeof(line),"Dep ns: S32 %lu P32 %lu P256 %lu",
      (unsigned long)memoryResults[0].dependentReadNs,(unsigned long)memoryResults[1].dependentReadNs,
      (unsigned long)memoryResults[2].dependentReadNs);
    textLine(228,line,0xffe0);
  }
}
void measureMemory() {
  beginBootStage(13);
  textLine(94,"Memory ns/op; HDMI ON, includes loop/IRQs");
  textLine(108,"Window       SeqR    DepR    SeqW   RandW");
  auto sram=emulator->bus().iwram();
  auto psram=emulator->bus().ewram();
  memoryResults[0]=benchmarkMemory(reinterpret_cast<volatile uint32_t*>(sram.data()),8192);
  watchdog_update();pollController();
  memoryResults[1]=benchmarkMemory(reinterpret_cast<volatile uint32_t*>(psram.data()),8192);
  watchdog_update();pollController();
  memoryResults[2]=benchmarkMemory(reinterpret_cast<volatile uint32_t*>(psram.data()),65536);
  watchdog_update();pollController();
  const char* names[]={"SRAM 32K","PSRAM32K","PSRAM256"};
  for(unsigned i=0;i<3;++i) {
    const auto& r=memoryResults[i];char line[80];
    snprintf(line,sizeof(line),"%-8s %7lu %7lu %7lu %7lu",names[i],
      (unsigned long)r.sequentialReadNs,(unsigned long)r.dependentReadNs,
      (unsigned long)r.sequentialWriteNs,(unsigned long)r.scatteredWriteNs);
    textLine(124+i*14,line,0xffe0);
  }
  textLine(174,"DepR: dependent 32-bit pointer-chain reads");
  textLine(188,"Photo these results; game starts in 15 sec");
  for(unsigned i=0;i<1500;++i) {
    serviceVideo();watchdog_update();pollController();busy_wait_ms(10);
  }
  // The normal reset below clears IWRAM/EWRAM before guest code executes.
}
void setup() {
  // Also retain diagnostics across a manual RESET if automatic recovery fails.
  const bool watchdogReset=watchdog_caused_reboot();
  const bool recovered=watchdog_hw->scratch[0]==BOOT_MAGIC;
  const uint32_t failedStage=watchdog_hw->scratch[1];
  const uint32_t fault=watchdog_hw->scratch[2],faultStatus=watchdog_hw->scratch[3];
  watchdog_disable();
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  safeCpuAndRamClock();
  Serial1.setRX(1);Serial1.setTX(0);Serial1.setFIFOSize(4096);Serial1.begin(230400);
  controller.clear();
  if(!display.begin())while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(100);}
  installVideoProbe();
  display.setTextWrap(false);display.fillScreen(0);
  textLine(8,"GBA RP2350 EXPERIMENT",0x07ff);
  videoReady=true;
  videoBackup=static_cast<uint8_t*>(gbaPsramAllocate(VIDEO_BYTES));
  if(!videoBackup)fail("Cannot allocate video recovery buffer");
  textLine(22,"BOOT v28 - SONIC / VIDEO WRITES",0x07e0);
  if(recovered) {
    textLine(48,"PREVIOUS RUN STOPPED AT:",0xf800);
    textLine(68,bootStageName(failedStage),0xffe0);
    textLine(92,fault?"CPU HARDFAULT":watchdogReset?"WATCHDOG: OPERATION DID NOT RETURN":"PREVIOUS RUN INTERRUPTED / MANUAL RESET");
    char detail[64];snprintf(detail,sizeof(detail),"Fault status: %08lX",(unsigned long)faultStatus);
    textLine(112,detail);
    textLine(144,"Report stage number and fault status.");
    watchdog_hw->scratch[0]=0;
    while(true){serviceVideo();recoveryLed();busy_wait_ms(10);}
  }
  exception_set_exclusive_handler(HARDFAULT_EXCEPTION,captureGbaFault);
  char bootInfo[70];
  snprintf(bootInfo,sizeof(bootInfo),"CPU %lu MHz / PSRAM %lu MiB",
    (unsigned long)(clock_get_hz(clk_sys)/1000000),
    (unsigned long)(rp2040.getPSRAMSize()/(1024*1024)));
  textLine(48,bootInfo);
  // Allow the monitor time to lock before any large memory allocation.
  delay(3000);
  if(rp2040.getPSRAMSize()<8*1024*1024)fail("Enable Tools > PSRAM > 8MByte");
  rom=new CompressedRom();
  if(!rom || !rom->ready())fail("Cannot allocate ROM cache in PSRAM");
  snprintf(bootInfo,sizeof(bootInfo),"Checking all %lu compressed ROM blocks...",(unsigned long)ROM_BLOCK_COUNT);
  textLine(36,bootInfo);
  beginBootStage(1);
  for(uint32_t page=0;page<ROM_BLOCK_COUNT;++page) {
    if(!rom->page(page) || rom->failed)fail("ROM inflate/CRC test FAILED");
    pollController();
    watchdog_update();
    if((page&31)==0){char line[60];snprintf(line,sizeof(line),"Verified %lu / %lu blocks",(unsigned long)page,(unsigned long)ROM_BLOCK_COUNT);textLine(64,line);}
  }
  textLine(64,"ROM CRC test PASSED",0x07e0);
  beginBootStage(12);
  unsigned retained=0;
  for(uint32_t page=0;page<ROM_BLOCK_COUNT;++page) {
    const uint8_t* data=rom->cached_page(page);
    if(data) {
      if(tinf_crc32(data,ROM_BLOCKS[page].unpacked)!=ROM_BLOCKS[page].crc)
        fail("Large PSRAM cache retention FAILED");
      ++retained;
    }
    pollController();watchdog_update();
  }
  if(retained!=std::min(size_t(ROM_BLOCK_COUNT),rom->backing_capacity()))
    fail("ROM cache residency mapping FAILED");
  textLine(64,"ROM cache retention PASSED",0x07e0);
  snprintf(bootInfo,sizeof(bootInfo),"SRAM free %d / core needs %lu",rp2040.getFreeHeap(),(unsigned long)sizeof(gba::Emulator));
  textLine(128,bootInfo);
  beginBootStage(2);
  gbaBootTraceHook=constructorProgress;
  gbaBootTrace(20);
  void* coreStorage=malloc(sizeof(gba::Emulator));
  if(!coreStorage)fail("Cannot allocate GBA interpreter SRAM");
  gbaBootTrace(35);
  emulator=new(coreStorage) gba::Emulator();
  gbaBootTrace(36);
  gbaBootTraceHook=nullptr;
  measureMemory();
  emulator->cartridge().set_scan_progress(cartridgeScanProgress);
  beginBootStage(3);
  emulator->set_rom_provider(std::unique_ptr<gba::RomProvider>(rom));
  // The scan's temporary heap buffer has now been destroyed. Reestablish the
  // video stream even if the DMA BUSY flags still look normal.
  restartVideo(true);
  beginBootStage(4);
  emulator->cartridge().auto_detect_save_type();
  beginBootStage(5);
  emulator->load_bios(std::vector<uint8_t>(std::begin(gba_bios_bin),std::end(gba_bios_bin)));
  beginBootStage(6);
  emulator->reset(false); // MIT-licensed replacement BIOS, no Nintendo BIOS dump.
  emulator->apu().set_synthesis_enabled(false);
  emulator->cpu().set_idle_acceleration(true);
  emulator->cpu().set_fast_fetch(true);
  // Restart after all save/BIOS allocations and bulk memory initialization.
  restartVideo(true);
  snprintf(bootInfo,sizeof(bootInfo),"PSRAM %lu /7680 KiB | SRAM free %d",(unsigned long)(gbaPsramUsed()/1024),rp2040.getFreeHeap());
  textLine(94,bootInfo,0x07e0);busy_wait_ms(2000);watchdog_update();
  beginBootStage(11);
  for(unsigned i=0;i<16;++i)renderStack[i]=RENDER_CANARY;
  multicore_launch_core1_with_stack(renderCoreMain,renderStack+16,8192);
  while(!renderCoreReady.load(std::memory_order_acquire))__wfe();
  emulator->set_render_backend(renderWorker.backend());
  display.fillScreen(0);
  // Match the RGB565 conversion of the PPU's reset-white framebuffer. Clean
  // rows retain their last display pixels, including after HDMI recovery.
  display.fillRect(40,48,240,160,0xffdf);
  renderWorker.set_video_target(display.getBuffer()+48*320+40,320);
  directVideoReady=true;
  textLine(34,"Experimental core / no audio / volatile saves",0x8410);
  textLine(228,"X=A O=B Options=Start Create=Select",0x8410);
  lastFps=millis();
  lastProfileMisses=rom->misses;lastProfileInflate=rom->inflateUs;
  beginBootStage(7);
  lastFps=millis();
  inputLatch.clear();
  if(controller.connected)inputLatch.update(gbaKeys(),emulatedFrames);
}
void loop() {
  pollController();
  runtimeProgress();
  emulator->set_keys(inputLatch.value(emulatedFrames));
  // Small emulated-cycle slices keep the UART receiver serviced between calls.
  const uint32_t started=micros();
  emulator->run_until(emulator->cpu().current_cycle()+1024);
  const bool frameReady=emulator->consume_frame_ready();
  runWallUs+=uint32_t(micros()-started);
  watchdog_update();
  if(rom->failed)fail("ROM decompression/CRC error during play");
  if(emulator->apu().audio_chunk_ready())(void)emulator->apu().consume_audio_chunk();
  if(frameReady) {
    if(!renderStackOK())fail("Core 1 render stack overflow");
    if(emulatedFrames==0) {
      // Preserve whether a failure occurs inside the first HDMI frame copy.
      watchdog_hw->scratch[1]=8;
    }
    presentGame();
    watchdog_hw->scratch[1]=9;
  }
}
