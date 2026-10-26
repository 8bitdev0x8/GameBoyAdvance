#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <algorithm>
#include "CompressedRom.h"
#include "gba/core/emulator.hpp"
#include "BiosStub.h"
#include "src/ReplacementBios.h"
#include "src/VideoIdleMonitor.h"
#include "src/FrameInputLatch.h"
#include "gba/core/render_worker.hpp"
#include "DebugLog.h"
#include "FrameBenchmark.h"

class HostRenderThread {
  gba::ScanlineWorker& worker_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
public:
  HostRenderThread(gba::ScanlineWorker& worker,bool enabled):worker_(worker) {
    if(enabled)thread_=std::thread([this]{while(!stop_.load())if(!worker_.run_one())std::this_thread::yield();});
  }
  ~HostRenderThread() {
    if(thread_.joinable()){worker_.wait();stop_.store(true);thread_.join();}
  }
};

const uint8_t* gba_rom_packed=nullptr;
struct PcSample { uint32_t count=0;gba::CpuState example; };
std::unordered_map<uint32_t,PcSample> pcSamples;
uint64_t pcSampleCount=0;
void samplePc(const gba::CpuState& state) {
  const uint32_t key=(state.regs[15]&~15u)|((state.cpsr&32u)?1u:0u);
  auto& s=pcSamples[key];if(!s.count)s.example=state;++s.count;++pcSampleCount;
}
std::vector<uint8_t> load(const char* path) {
  std::ifstream f(path,std::ios::binary);
  if(!f)throw std::runtime_error("Cannot open test input");
  return {std::istreambuf_iterator<char>(f),{}};
}
void check(bool condition,const char* what) {if(!condition)throw std::runtime_error(what);}
uint32_t hashFrame(std::span<const uint16_t> frame) {
  uint32_t h=2166136261u;
  for(uint16_t p:frame) {h=(h^uint8_t(p))*16777619u;h=(h^uint8_t(p>>8))*16777619u;}
  return h;
}
void writeFrame(const std::string& path,std::span<const uint16_t> frame) {
  std::ofstream ppm(path,std::ios::binary);ppm<<"P6\n240 160\n255\n";
  for(uint16_t v:frame) {
    char rgb[3]={char((v&31)*255/31),char(((v>>5)&31)*255/31),char(((v>>10)&31)*255/31)};
    ppm.write(rgb,3);
  }
}
size_t scanCompleted=0,scanCalls=0;
void scanProgress(size_t completed,size_t total) {
  check(total==ROM_SIZE && completed>scanCompleted && completed<=total,"Scan progress bounds/order");
  scanCompleted=completed;++scanCalls;
}
void testRamWait() {
  for(uint32_t ram:{0x02000000u,0x03000000u,0x04000130u}) {
    for(uint16_t wait:{uint16_t(0),uint16_t(0x4317)}) {
      gba::Emulator plain,fast;
      std::vector<uint8_t> code(512);
      const uint16_t ops[]={0x8811,0x1c18,0x4008,0x2800,0xd0fa,0x2407,0xe7fe};
      memcpy(code.data()+0x100,ops,sizeof(ops));
      plain.load_rom(code);fast.load_rom(code);
      for(auto* emu:{&plain,&fast}) {
        emu->reset(true);
        auto& s=emu->cpu().state();
        s.cpsr=0x3f; s.regs[15]=0x08000100; s.regs[2]=ram;
        s.regs[3]=ram==0x04000130u?0x8000:1;
        (void)emu->bus().write(0x04000204,wait,gba::BusWidth::Half,gba::AccessType::Io,0);
      }
      fast.cpu().set_idle_acceleration(true);
      for(unsigned i=0;i<200;++i) {
        const auto target=plain.cpu().current_cycle()+(i%3==0?1024:97);
        plain.run_until(target);fast.run_until(target);
        check(plain.cpu().state()==fast.cpu().state(),"Synthetic wait CPU parity");
        check(plain.cpu().current_cycle()==fast.cpu().current_cycle(),"Synthetic wait cycle parity");
        check(plain.bus().idle_bus_state()==fast.bus().idle_bus_state(),"Synthetic wait bus parity");
        if(i==100 && ram!=0x04000130u) {
          for(auto* emu:{&plain,&fast})
            (void)emu->bus().write(ram,1,gba::BusWidth::Half,gba::AccessType::NonSequential,emu->cpu().current_cycle());
        }
      }
      if(ram==0x04000130u)check(fast.cpu().idle_skipped_cycles()==0,"Never accelerate MMIO polling");
      else {
        check(fast.cpu().idle_skipped_cycles()>0,"Synthetic RAM wait accelerated");
        check(fast.cpu().state().regs[4]==7,"RAM flag change resumes normal execution");
      }
    }
  }
  std::cout<<"PASS: RAM wait event slices, EWRAM/IWRAM, prefetch on/off, flag wake, MMIO rejection\n";
}
void testVblankWait() {
  for(bool nonzero:{false,true})for(uint16_t wait:{uint16_t(0),uint16_t(0x4317)})
  for(bool interrupts:{false,true})for(uint32_t mask:{1u,2u}) {
    gba::Emulator plain,fast;
    std::vector<uint8_t> code(512);
    const uint32_t stay=0xeafffffe;memcpy(code.data(),&stay,4);
    const uint16_t ops[]={0x8811,0x1c18,0x4008,0x2800,uint16_t(nonzero?0xd1fa:0xd0fa),0x2407,0xe7fe};
    memcpy(code.data()+0x100,ops,sizeof(ops));
    // IRQ callback acknowledges IF, then returns through the BIOS trampoline.
    const uint32_t irq[]={0xe3a00301,0xe2800c02,0xe3e01000,0xe1c010b2,0xe12fff1e};
    memcpy(code.data()+0x180,irq,sizeof(irq));
    for(auto* emu:{&plain,&fast}) {
      emu->load_rom(code);emu->load_bios(makeBiosStub());emu->reset(true);
      if(nonzero)emu->run_until(160u*1232u+256u);
      auto& s=emu->cpu().state();s.cpsr=0x3f;s.regs[15]=0x08000100;
      s.regs[2]=0x04000004;s.regs[3]=mask;
      const auto now=emu->cpu().current_cycle();
      auto write=[&](uint32_t a,uint32_t value,gba::BusWidth width=gba::BusWidth::Half) {
        (void)emu->bus().write(a,value,width,gba::AccessType::Io,now);
      };
      write(0x04000204,wait);
      if(interrupts) {
        write(0x03007ffc,0x08000180,gba::BusWidth::Word);
        write(0x04000004,0x18);write(0x04000200,0x0b);write(0x04000208,1);
        write(0x04000100,0xff00);write(0x04000102,0xc0);
      }
    }
    fast.cpu().set_idle_acceleration(true);
    for(unsigned i=0;i<700;++i) {
      const auto target=plain.cpu().current_cycle()+(i%4==0?17:i%4==1?97:i%4==2?1024:4096);
      plain.run_until(target);fast.run_until(target);
      check(plain.cpu().state()==fast.cpu().state(),"VBlank wait CPU/banked/IRQ parity");
      check(plain.cpu().current_cycle()==fast.cpu().current_cycle(),"VBlank wait exact cycle parity");
      check(plain.bus().idle_bus_state()==fast.bus().idle_bus_state(),"VBlank wait bus/prefetch parity");
      check(plain.ppu().vcount()==fast.ppu().vcount() && plain.ppu().dispstat()==fast.ppu().dispstat(),"VBlank wait PPU status parity");
      check(plain.irq().ie()==fast.irq().ie() && plain.irq().iflags()==fast.irq().iflags() && plain.irq().ime()==fast.irq().ime(),"VBlank wait interrupt-controller parity");
    }
    if(mask==1)check(fast.cpu().vblank_skipped_cycles()>0,"VBlank-bit loop accelerated");
    else check(fast.cpu().idle_skipped_cycles()==0,"Other DISPSTAT bits are not accelerated");
    check(fast.cpu().state().regs[4]==7,"VBlank transition exits polling loop");
  }
  std::cout<<"PASS: BEQ/BNE VBlank waits, event boundary slices, prefetch on/off, HBlank/VBlank/timer IRQs, exact CPU/bus/PPU parity and other-status-mask rejection\n";
}
void testDirectWorker() {
  gba::Ppu ppu;ppu.reset();
  gba::ScanlineWorker worker;
  auto backend=worker.backend();
  std::vector<uint8_t> vram(gba::kVramSize),palette(gba::kPaletteSize),oam(gba::kOamSize);
  std::vector<uint16_t> first(320*240,0xdead),second(320*240,0xbeef);
  uint32_t elapsed=0;
  auto submit=[&](int line) { backend.submit(backend.context,ppu,line,vram,palette,oam,&elapsed); };
  worker.set_video_target(first.data()+48*320+40,320);
  submit(0);check(worker.run_one(),"Dirty row dispatched");worker.wait();
  for(unsigned x=0;x<320;++x)
    check(first[48*320+x]==(x>=40 && x<280?0xffdf:0xdead),"Forced blank converts to RGB565 without touching margins");
  const auto completed=worker.completed();
  submit(0);check(worker.run_one() && !worker.run_one() && worker.completed()==completed,"Queued clean row is consumed without rendering or copying");
  ppu.mark_dirty(1);submit(1);
  std::atomic<bool> rebinding{false};
  std::thread rebind([&] { rebinding.store(true,std::memory_order_release);worker.set_video_target(nullptr,0); });
  while(!rebinding.load(std::memory_order_acquire))std::this_thread::yield();
  const bool finished=worker.run_one();rebind.join();
  check(finished,"Pending row finished before target detach");
  for(unsigned x=40;x<280;++x)check(first[49*320+x]==0xffdf,"Pending job used old live allocation");
  std::fill(first.begin(),first.end(),0xaaaa); // Model retired display storage.
  worker.set_video_target(second.data()+48*320+40,320);
  ppu.mark_dirty(2);submit(2);check(worker.run_one(),"Row dispatched after target rebind");worker.wait();
  check(std::all_of(first.begin(),first.end(),[](uint16_t v){return v==0xaaaa;}),"Rebound worker never writes retired target");
  for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x)
    check(second[y*320+x]==(y==50 && x>=40 && x<280?0xffdf:0xbeef),"Rebound output preserves other rows and margins");
  // Multiple queued jobs must retain FIFO order and clean duplicates must
  // leave the previous pixels intact. Exercise ring wrap over 64 batches.
  worker.set_video_target(first.data()+48*320+40,320);
  for(unsigned batch=0;batch<64;++batch) {
    worker.wait();
    const int base=int((batch*3)%150);
    for(int row=base;row<base+3;++row)ppu.mark_dirty(row);
    const auto before=worker.completed();
    submit(base);submit(base+1);submit(base+2);submit(base+2);
    for(unsigned job=0;job<4;++job)check(worker.run_one(),"Four queued jobs consumed");
    check(!worker.run_one() && worker.completed()==before+3,"Duplicate queued row avoids stale row copy");
    worker.wait();
    for(int row=base;row<base+3;++row)for(unsigned x=40;x<280;++x)
      check(first[(row+48)*320+x]==0xffdf,"Queued batch pixels survive ring wrap");
  }
  std::cout<<"PASS: queued worker forced blank, clean-row suppression, ring wrap, duplicate line, pending-job detach and target lifetime/rebind\n";
}
void testTileCacheWrap() {
  for(bool eightBit:{false,true}) {
  gba::Ppu ppu;ppu.reset();
  std::vector<uint8_t> vram(gba::kVramSize),palette(gba::kPaletteSize),oam(gba::kOamSize);
  std::fill_n(vram.begin(),eightBit?64:32,uint8_t(eightBit?1:0x11));
  palette[2]=31;palette[4]=0xe0;palette[5]=3;
  ppu.write_register(gba::kDispcnt,0x0100,gba::BusWidth::Half);
  ppu.write_register(gba::kBg0Cnt,0x0100|(eightBit?0x80:0),gba::BusWidth::Half);
  ppu.render_scanline(0,vram,palette,oam);
  check(ppu.framebuffer()[0]==31,"Initial decoded tile color");
  std::fill_n(vram.begin(),eightBit?64:32,uint8_t(eightBit?2:0x22));
  for(unsigned i=0;i<4096;++i)ppu.mark_all_dirty();
  ppu.render_scanline(0,vram,palette,oam);
  check(ppu.framebuffer()[0]==0x03e0,"Tile cache must not reuse stale pixels after 4096 invalidations");
  }
  std::cout<<"PASS: 4bpp/8bpp tile cache invalidation wraparound\n";
}
void testVideoInvalidation() {
  gba::Ppu ppu;ppu.reset();
  std::vector<uint8_t> vram(gba::kVramSize),palette(gba::kPaletteSize),oam(gba::kOamSize);
  auto half=[](auto& bytes,unsigned offset,uint16_t value) {bytes[offset]=uint8_t(value);bytes[offset+1]=uint8_t(value>>8);};
  std::fill_n(vram.begin(),32,uint8_t(0x11));
  half(palette,2,31);half(palette,4,0x7c00);
  for(unsigned i=0;i<128;++i)half(oam,i*8,0x0200);
  ppu.write_register(gba::kDispcnt,0x1100,gba::BusWidth::Half);
  ppu.write_register(gba::kBg0Cnt,0x0100,gba::BusWidth::Half);
  ppu.render_scanline(0,vram,palette,oam);
  check(ppu.framebuffer()[0]==31,"Initial video invalidation fixture");
  half(palette,2,0x03e0);ppu.mark_memory_dirty(0x05000000);
  ppu.render_scanline(0,vram,palette,oam);
  check(ppu.framebuffer()[0]==0x03e0,"Palette updates recolor cached tile indices");
  std::fill_n(vram.begin(),32,uint8_t(0x22));ppu.mark_memory_dirty(0x06000000);
  ppu.render_scanline(0,vram,palette,oam);
  check(ppu.framebuffer()[0]==0x7c00,"VRAM updates invalidate decoded tile indices");
  std::fill_n(vram.begin()+0x10000,32,uint8_t(0x33));half(palette,0x206,31);
  half(oam,0,0);ppu.mark_memory_dirty(0x07000000);
  ppu.render_scanline(0,vram,palette,oam);
  check(ppu.framebuffer()[0]==31,"OAM updates invalidate sprite scanline membership");
  std::fill_n(vram.begin()+0x10000,32,uint8_t(0x44));half(palette,0x208,0x03e0);
  ppu.mark_memory_dirty(0x06000000);ppu.render_scanline(0,vram,palette,oam);
  check(ppu.framebuffer()[0]==0x03e0,"Sprite pixels update without changing membership");
  std::cout<<"PASS: independent palette, VRAM and OAM invalidation\n";
}
void testTextMapBlocks() {
  for(unsigned size=0;size<4;++size) {
    gba::Ppu ppu;ppu.reset();
    std::vector<uint8_t> vram(gba::kVramSize),palette(gba::kPaletteSize),oam(gba::kOamSize);
    std::fill_n(vram.begin(),32,uint8_t(0x11));
    std::fill_n(vram.begin()+32,32,uint8_t(0x22));
    palette[2]=31;palette[4]=0xe0;palette[5]=3;
    const unsigned block=size==3?3:(size==0?0:1);
    vram[0x800+block*0x800]=1;
    ppu.write_register(gba::kDispcnt,0x0100,gba::BusWidth::Half);
    ppu.write_register(gba::kBg0Cnt,0x0100|(size<<14),gba::BusWidth::Half);
    ppu.write_register(0x04000010,(size&1)?256:0,gba::BusWidth::Half);
    ppu.write_register(0x04000012,(size&2)?256:0,gba::BusWidth::Half);
    ppu.render_scanline(0,vram,palette,oam);
    check(ppu.framebuffer()[0]==0x03e0,"Text background must select the correct screen block for each size");
  }
  std::cout<<"PASS: text background 32x32, 64x32, 32x64 and 64x64 block addressing\n";
}
void testInputLatch() {
  FrameInputLatch latch;
  latch.update(1,100);latch.update(0,100);
  check(latch.value(100)==1 && latch.value(101)==1,"Short press survives a full guest frame");
  check(latch.value(102)==0,"Released press expires without sticking");
  latch.update(1<<3,103);
  check(latch.value(1000)==(1<<3),"Physically held button stays held");
  latch.update(0,1000);check(latch.value(1000)==0,"Long-held button releases immediately");
  latch.update(1,0xffffffffu);latch.update(0,0xffffffffu);
  check(latch.value(0)==1 && latch.value(1)==0,"Frame counter rollover");
  latch.update(1,2);latch.clear();check(latch.value(2)==0,"Disconnect clears pending input");
  std::cout<<"PASS: short UART press delivery, release, held key, disconnect and frame rollover\n";
}
void testSaveStorage() {
  gba::Cartridge cart;
  cart.set_save_type(gba::SaveType::Eeprom);
  const auto* reserved=cart.save().data();
  for(auto type:{gba::SaveType::Flash64K,gba::SaveType::Flash128K,gba::SaveType::Sram,gba::SaveType::Eeprom}) {
    cart.set_save_type(type);
    const size_t expected=type==gba::SaveType::Eeprom?8192:type==gba::SaveType::Flash128K?131072:65536;
    check(cart.save().size()==expected,"Save type size");
    check(cart.save().data()==reserved,"Save type changes reuse single maximum-sized PSRAM reservation");
    check(std::all_of(cart.save().begin(),cart.save().end(),[](uint8_t v){return v==255;}),"Erased save memory");
  }
  cart.set_save_type(gba::SaveType::Flash64K);
  auto write=[&](uint32_t a,uint8_t v){cart.write_save(a,v,gba::BusWidth::Byte);};
  auto command=[&](uint8_t v){write(0x5555,0xaa);write(0x2aaa,0x55);write(0x5555,v);};
  command(0x90);check(cart.read_save(0,gba::BusWidth::Byte)==0xbf && cart.read_save(1,gba::BusWidth::Byte)==0xd4,"Flash64 chip ID");
  command(0xf0);command(0xa0);write(0x1234,0x5a);
  check(cart.read_save(0x1234,gba::BusWidth::Byte)==0x5a && cart.is_save_dirty(),"Flash64 byte program");
  command(0x80);write(0x5555,0xaa);write(0x2aaa,0x55);write(0x1000,0x30);
  check(cart.read_save(0x1234,gba::BusWidth::Byte)==255,"Flash64 sector erase");
  cart.load_save(std::vector<uint8_t>(65536,0x3c));
  check(cart.save().data()==reserved && cart.read_save(0x1234,gba::BusWidth::Byte)==0x3c,"Load save reuses PSRAM storage");
  std::cout<<"PASS: maximum save allocation reused, EEPROM/SRAM/Flash sizes, Flash64 ID/program/erase/load\n";
}
void testSilentApu() {
  gba::Apu audible,silent;audible.reset();silent.reset();silent.set_synthesis_enabled(false);
  std::mt19937 rng(2202350);
  const uint32_t registers[]={0x60,0x62,0x64,0x68,0x6c,0x70,0x72,0x74,0x78,0x7c,0x80,0x82,0x84,0x88,0x90,0x92,0x94,0x96,0x98,0x9a,0x9c,0x9e,0xa0,0xa4};
  for(uint64_t cycle=0;cycle<2000000;cycle+=137) {
    if(rng()%9==0) {
      const auto address=0x04000000+registers[rng()%std::size(registers)];
      const auto value=rng();
      audible.write_register(address,value,gba::BusWidth::Half,cycle);
      silent.write_register(address,value,gba::BusWidth::Half,cycle);
    }
    audible.advance_to(cycle);silent.advance_to(cycle);
    if(rng()%3==0) {
      const int timer=rng()%2;audible.on_timer_overflow(timer);silent.on_timer_overflow(timer);
    }
    check(audible.next_event_cycle()==silent.next_event_cycle(),"Silent APU keeps sample/frame event times");
    check(audible.take_fifo_request_a()==silent.take_fifo_request_a() && audible.take_fifo_request_b()==silent.take_fifo_request_b(),"Silent APU keeps FIFO DMA requests");
    for(auto offset:registers)
      check(audible.read_register(0x04000000+offset,gba::BusWidth::Half)==silent.read_register(0x04000000+offset,gba::BusWidth::Half),"Silent APU keeps guest-visible sound state");
    if(audible.audio_chunk_ready())(void)audible.consume_audio_chunk();
    check(!silent.audio_chunk_ready(),"Silent frontend does not synthesize discarded samples");
  }
  std::cout<<"PASS: silent APU preserves register/status reads, FIFO DMA requests and event times under randomized sound writes\n";
}
int main(int argc,char**argv) {
  try {
    if(argc!=4)throw std::runtime_error("Arguments: original.gba romblocks.bin output.ppm");
    testRamWait();
    testVblankWait();
    testDirectWorker();
    testTextMapBlocks();
    testTileCacheWrap();
    testVideoInvalidation();
    testInputLatch();
    testSaveStorage();
    testSilentApu();
    VideoIdleMonitor idle;
    check(!idle.stopped(100,true) && !idle.stopped(110,false) && !idle.stopped(129,false),"Transient DMA idle does not restart video");
    check(idle.stopped(130,false),"Persistent DMA idle triggers recovery");
    check(!idle.stopped(131,true) && !idle.stopped(132,false),"Active DMA resets idle timer");
    idle.reset();
    check(!idle.stopped(0xfffffff0u,false) && idle.stopped(4,false),"DMA idle timer handles wraparound");
    std::cout<<"PASS: video DMA idle timing and timer wrap\n";
    VideoProgressMonitor videoProgress;
    check(!videoProgress.stalled(100,1) && !videoProgress.stalled(199,1),"Video progress timeout grace period");
    check(videoProgress.stalled(200,1),"No completed video IRQ triggers recovery even with DMA busy");
    check(!videoProgress.stalled(201,2) && !videoProgress.stalled(300,3),"Completed video IRQ resets timeout");
    videoProgress.reset();
    check(!videoProgress.stalled(0xfffffff0u,0xffffffffu) && !videoProgress.stalled(4,0),"IRQ counter wrap is progress");
    check(videoProgress.stalled(104,0),"Video progress timeout handles time wrap");
    std::cout<<"PASS: video IRQ progress timeout and counter/time wrap\n";
    VideoRateMonitor videoRate;
    VideoRateMonitor frameRate{45,75};
    check(!frameRate.invalid(0,0) && !frameRate.invalid(500,30) && !frameRate.invalid(1000,60),"Healthy DMA ring frame IRQ rate");
    check(!frameRate.invalid(1500,15810) && frameRate.invalid(2000,31560),"Line-rate IRQs invalid in frame-ring mode");
    check(!videoRate.invalid(0,0) && !videoRate.invalid(500,15750) && !videoRate.invalid(1000,31500),"Healthy HDMI line rate");
    check(!videoRate.invalid(1500,73500) && videoRate.invalid(2000,115500),"V48 84 kHz malformed stream triggers recovery");
    videoRate.reset();
    check(!videoRate.invalid(0,0) && !videoRate.invalid(500,42000) && !videoRate.invalid(1000,57750) && !videoRate.invalid(1500,99750),"Healthy window clears transient rate error");
    check(!videoRate.invalid(5000,200000) && !videoRate.invalid(5500,242000) && videoRate.invalid(6000,284000),"Long startup pauses rebaseline video rate");
    videoRate.reset();
    check(!videoRate.invalid(0xffffff00u,0xffffe000u) && !videoRate.invalid(244,7558) && !videoRate.invalid(744,23308),"Video rate handles time and IRQ count wrap");
    std::cout<<"PASS: HDMI line-rate recovery, transient rejection, startup pauses and counter wrap\n";
    std::vector<uint8_t> arenaBuffer(GBA_PSRAM_ARENA_BYTES);
    GbaArena bounded(arenaBuffer.data(),arenaBuffer.size());
    // Static SRAM holds IWRAM, VRAM, palette, OAM, wait tables and hot ROM lines.
    std::vector<uint8_t> fastBuffer(GBA_FAST_BUS_BYTES);
    GbaArena fast(fastBuffer.data(),fastBuffer.size());
    for(size_t n:{size_t(gba::kIwramSize),size_t(gba::kPaletteSize),size_t(gba::kVramSize),size_t(gba::kOamSize),sizeof(gba::Bus::WaitStateTables)})
      check(fast.allocate(n)!=nullptr,"Static SRAM pool capacity");
    check(fast.used()==GBA_FAST_BUS_BYTES && !fast.allocate(1),"Exact SRAM pool budget");
    // Full external-memory budget, including debug capture and cold host PCM.
    for(size_t n:{size_t(320*240*2),GBA_ROM_BACKING_BYTES,size_t(gba::kFramebufferPixels*2),
        size_t(gba::kEwramSize),GBA_SAVE_CAPACITY}) {
      void* p=bounded.allocate(n);
      check(p && bounded.owns(p) && !(reinterpret_cast<uintptr_t>(p)&15),"Bounded arena allocation/alignment");
    }
    check(bounded.allocate(GbaDebug::StorageBytes)!=nullptr,"Debug ring fits beside largest cartridge save");
#if defined(GBA_PLATFORM_RP2350) && !defined(GBA_APU_INTERNAL_MIX_BUFFER)
    check(bounded.allocate(GBA_APU_MIX_BYTES)!=nullptr,"Cold audio buffer fits beside largest save and debug ring");
#endif
    const size_t before=bounded.used();
#if defined(GBA_PLATFORM_RP2350) && !defined(GBA_APU_INTERNAL_MIX_BUFFER)
    check(before==7848960 && GBA_PSRAM_ARENA_BYTES-before==15360,"Complete ROM/audio/debug/128 KiB save arena budget");
#else
    check(before==7857152 && GBA_PSRAM_ARENA_BYTES-before==7168,"Complete ROM/debug/128 KiB save arena budget");
#endif
    check(!bounded.allocate(GBA_PSRAM_ARENA_BYTES) && bounded.used()==before,"Arena bounds/failure atomicity");
    std::cout<<"PASS: fixed 7.5 MiB arena capacity, alignment, overflow rejection\n";
    auto original=load(argv[1]),packed=load(argv[2]);
    gba_rom_packed=packed.data();
    check(original.size()==ROM_SIZE && packed.size()==ROM_PACKED_SIZE,"Asset sizes");
    CompressedRom loader;
    check(loader.ready(),"Cache allocation");
    for(uint32_t i=0;i<ROM_BLOCK_COUNT;++i) {
      const uint8_t* page=loader.page(i);
      check(page && !loader.failed,"Page CRC/decode");
      check(!memcmp(page,original.data()+i*ROM_PAGE_SIZE,ROM_BLOCKS[i].unpacked),"Exact page bytes");
    }
    std::mt19937 random(2350);
    for(unsigned i=0;i<10000;++i) {
      uint32_t a=random()%(ROM_SIZE-4);
      uint16_t half;uint32_t word;
      memcpy(&half,original.data()+a,2);memcpy(&word,original.data()+a,4);
      check(loader.read16(a)==half && loader.read32(a)==word,"Random reads");
    }
    for(uint32_t a=ROM_PAGE_SIZE-3;a<ROM_SIZE-4;a+=ROM_PAGE_SIZE) {
      uint32_t word;memcpy(&word,original.data()+a,4);
      check(loader.read32(a)==word,"Cross-page word read");
    }
    const auto fastAccess=loader.fast_access();
    check(fastAccess.has_functions() && fastAccess.read16 && fastAccess.read32,"Direct compressed-provider callbacks");
    for(uint32_t boundary=256;boundary<ROM_SIZE-4;boundary+=256) {
      const uint32_t a=boundary-3;
      uint32_t word;memcpy(&word,original.data()+a,4);
      check(loader.read32(a)==word && fastAccess.read32(fastAccess.context,a)==word,"SRAM ROM line boundary read");
    }
    const uint32_t hotMisses=loader.hotMisses;
    for(unsigned i=0;i<1000;++i)(void)fastAccess.read32(fastAccess.context,0);
    check(loader.hotMisses<=hotMisses+1,"Repeated instruction fetches hit SRAM hot cache");
    for(uint32_t a:{0u,32768u,0u,ROM_SIZE-4}) {
      uint32_t word;uint16_t half;memcpy(&word,original.data()+a,4);memcpy(&half,original.data()+a,2);
      check(fastAccess.read_byte(fastAccess.context,a)==original[a] && fastAccess.read16(fastAccess.context,a)==half && fastAccess.read32(fastAccess.context,a)==word,"SRAM conflict eviction and direct callbacks");
    }
    (void)loader.read32(0);(void)loader.read32(32768);
    const uint32_t conflictMisses=loader.hotMisses;
    for(unsigned i=0;i<1000;++i){(void)loader.read32(0);(void)loader.read32(32768);}
    check(loader.hotMisses==conflictMisses,"Two-way SRAM cache keeps colliding code/data lines");
    std::cout<<"PASS: SRAM ROM cache line boundaries, direct callbacks, hits and two-way conflicts\n";
    check(fastAccess.code_line!=nullptr,"Compressed ROM opts into stable instruction lines");
    const uint8_t* stableCode=fastAccess.code_line(fastAccess.context,256);
    check(stableCode && std::equal(stableCode,stableCode+256,original.data()+256),"Instruction line initially matches source ROM");
    for(unsigned i=0;i<128;++i)(void)loader.read32((i*32768u)%ROM_SIZE);
    check(std::equal(stableCode,stableCode+256,original.data()+256),"Instruction line survives evicting ordinary data-cache reads");
    const uint8_t* nextCode=fastAccess.code_line(fastAccess.context,512);
    check(nextCode && std::equal(nextCode,nextCode+256,original.data()+512),"Explicit instruction refill changes window safely");
    check(!fastAccess.code_line(fastAccess.context,1) && !fastAccess.code_line(fastAccess.context,ROM_SIZE),"Invalid instruction line requests fail safely");
    std::cout<<"PASS: instruction window survives ordinary data-cache eviction and matches ROM after explicit refill\n";
    for(unsigned i=0;i<32;++i)(void)loader.page(i*16);
    const auto backingMisses=loader.misses;
    for(unsigned pass=0;pass<16;++pass)for(unsigned i=0;i<32;++i)
      check(loader.page(i*16)[0]==original[i*16*ROM_PAGE_SIZE],"Associative cache page mapping");
    check(loader.misses==backingMisses,"Aliasing ROM pages remain resident in associative cache");
    std::cout<<"PASS: associative backing cache avoids direct-map conflicts\n";
    check(loader.read_byte(ROM_SIZE)==255,"Out of bounds");
    { // Corrupt a block before the first read; must fail instead of serving bad bytes.
      packed[0]^=0x80;
      CompressedRom corrupt;
      check(corrupt.page(0)==nullptr && corrupt.failed,"Corruption rejection");
      packed[0]^=0x80;
    }
    std::cout<<"PASS: all "<<ROM_BLOCK_COUNT<<" blocks, 10000 random reads, boundaries, corrupt input\n";
    for(unsigned addressBits:{6u,14u}) {
      gba::Cartridge eeprom;eeprom.set_save_type(gba::SaveType::Eeprom);
      const unsigned address=addressBits==6 ? 37 : 723;
      auto bit=[&](unsigned v){eeprom.eeprom_write_bit(v);};
      auto command=[&](bool read){
        eeprom.eeprom_begin_dma(0x02000000,0x0d000000,2+addressBits+(read?0:64)+1);
        bit(1);bit(read?1:0);
        for(int i=int(addressBits)-1;i>=0;--i)bit((address>>i)&1);
      };
      const uint64_t pattern=0x0123456789abcdefULL;
      command(false);for(int i=63;i>=0;--i)bit((pattern>>i)&1);bit(0);
      check(eeprom.eeprom_read_bit()==1,"EEPROM write-ready");
      command(true);bit(0);
      eeprom.eeprom_begin_dma(0x0d000000,0x02000000,68);
      for(unsigned i=0;i<4;++i)check(eeprom.eeprom_read_bit()==0,"EEPROM dummy bits");
      uint64_t restored=0;
      for(unsigned i=0;i<64;++i)restored=(restored<<1)|eeprom.eeprom_read_bit();
      check(restored==pattern,"EEPROM serial write/read");
    }
    std::cout<<"PASS: EEPROM 6/14-bit addressing and serial round-trip\n";
    const bool threaded=getenv("GBA_TEST_THREADED")!=nullptr;
    const bool directVideo=getenv("GBA_TEST_DIRECT_VIDEO")!=nullptr;
    const bool renderProfiling=getenv("GBA_TEST_RENDER_PROFILE")!=nullptr;
    check(!directVideo || threaded,"Direct output requires threaded renderer");
    check(!renderProfiling || threaded,"Render profiling requires worker backend");
    std::vector<uint16_t> videoPixels;
    gba::ScanlineWorker worker;
    worker.set_profile_enabled(renderProfiling);
    gba::RenderProfile previousRenderProfile{};
    HostRenderThread renderThread(worker,threaded);
    std::vector<uint8_t> borrowedBios; // External view outlives both emulators.
    gba::Emulator memory,compressed;
    memory.cpu().set_fast_memory(false);
    memory.cpu().optimize_thumb_alu=false;
    compressed.cpu().set_idle_acceleration(true);
    compressed.cpu().set_fast_fetch(true);
    compressed.cpu().set_block_execution(false);
    if(getenv("GBA_TEST_NATIVE_MODEL")) {
      gba::Arm7tdmi::native_executor=[](uint16_t*,uint32_t,uint32_t*,uint32_t*,void* c){gba::Arm7tdmi::native_test_model(c);};
      compressed.cpu().set_native_execution(true);
    }
    memory.bus().optimize_video_writes=false;
    memory.cartridge().optimize_bios_reads=false;
    memory.ppu().optimize_video_writes=false;
    memory.load_rom(original);
    auto provider=std::make_unique<CompressedRom>();auto* stats=provider.get();
    compressed.cartridge().set_scan_progress(scanProgress);
    compressed.set_rom_provider(std::move(provider));
    const auto expectedScanCalls=(ROM_SIZE+1023u)/1024u;
    check(scanCompleted==ROM_SIZE && scanCalls==expectedScanCalls,"Scan progress covers every 1 KiB ROM chunk");
    memory.cartridge().auto_detect_save_type();compressed.cartridge().auto_detect_save_type();
    check(scanCalls==expectedScanCalls,"Cached feature scan does not repeat callbacks");
    check(compressed.cartridge().save_type()==memory.cartridge().save_type() && compressed.cartridge().rtc_enabled()==memory.cartridge().rtc_enabled(),"Save/RTC detection matches original ROM");
    std::cout<<"PASS: cartridge scan progress, cached scan, save/RTC detection; save type "<<int(compressed.cartridge().save_type())<<"\n";
    const char* localBiosPath=getenv("GBA_TEST_BIOS_PATH");
    bool replacement=localBiosPath || getenv("GBA_TEST_HLE") == nullptr;
    if(localBiosPath) {
      auto bios=load(localBiosPath);
      check(bios.size()==16384,"Local GBA BIOS is exactly 16 KiB");
      std::cout<<"Local BIOS: 16384 bytes, SWI vector 0x"<<std::hex
               <<(unsigned(bios[8])|(unsigned(bios[9])<<8)|(unsigned(bios[10])<<16)|(unsigned(bios[11])<<24))
               <<std::dec<<"\n";
      memory.load_bios(bios);
      if(getenv("GBA_TEST_BIOS_VIEW")) {
        borrowedBios=std::move(bios);
        compressed.cartridge().set_bios_view(borrowedBios);
        std::cout<<"External BIOS view enabled; conservative reference retains owned copy\n";
      } else compressed.load_bios(std::move(bios));
    } else if(replacement) {
      memory.load_bios(std::vector<uint8_t>(std::begin(gba_bios_bin),std::end(gba_bios_bin)));
      compressed.load_bios(std::vector<uint8_t>(std::begin(gba_bios_bin),std::end(gba_bios_bin)));
    } else {memory.load_bios(makeBiosStub());compressed.load_bios(makeBiosStub());}
    memory.reset(!replacement);compressed.reset(!replacement);
    if(getenv("GBA_TEST_PC_PROFILE"))compressed.cpu().profile_hook=samplePc;
    if(getenv("GBA_TEST_MUTE"))compressed.apu().set_synthesis_enabled(false);
    if(directVideo) {
      videoPixels.assign(320*240,0xdead);
      for(unsigned y=48;y<208;++y)std::fill_n(videoPixels.data()+y*320+40,240,0xffdf);
      worker.set_video_target(videoPixels.data()+48*320+40,320);
    }
    if(threaded)compressed.set_render_backend(worker.backend());
    const char* binaryLogPath=getenv("GBA_TEST_BINARY_LOG");
    check(!binaryLogPath || renderProfiling,"Binary log test requires render profiling");
    GbaDebug::Recorder recorder;
    std::vector<uint8_t> logStorage(binaryLogPath?GbaDebug::StorageBytes:0);
    recorder.bind(logStorage);
    auto captureLogCounters=[&](uint32_t frame) {
      GbaDebug::Counters c{};const auto p=worker.profile_snapshot();
      c[GbaDebug::FrameNo]=frame;c[GbaDebug::WallUs]=micros();c[GbaDebug::UptimeMs]=micros()/1000;
      c[GbaDebug::DrawUs]=compressed.render_wall_us();c[GbaDebug::WaitUs]=worker.wait_wall_us();
      c[GbaDebug::QfullUs]=worker.full_wait_wall_us();c[GbaDebug::BarrierUs]=worker.barrier_wait_wall_us();
      c[GbaDebug::PrepUs]=p.stages.setup_us;c[GbaDebug::BgUs]=p.stages.background_us;
      c[GbaDebug::ObjUs]=p.stages.object_us;c[GbaDebug::MixUs]=p.stages.composite_us;
      c[GbaDebug::RgbUs]=p.stages.rgb_us;c[GbaDebug::OtherUs]=p.other_us;c[GbaDebug::WorkerUs]=p.total_us;
      c[GbaDebug::Jobs]=p.jobs;c[GbaDebug::DirtyJobs]=p.dirty_jobs;
      c[GbaDebug::RomMisses]=stats->misses;c[GbaDebug::DecodeUs]=stats->inflateUs;
      c[GbaDebug::IFetchHits]=compressed.cartridge().instruction_hits();
      c[GbaDebug::IFetchFills]=compressed.cartridge().instruction_fills();
      c[GbaDebug::GuestCycles]=uint32_t(compressed.cpu().current_cycle());
      c[GbaDebug::VblankCycles]=uint32_t(compressed.cpu().vblank_skipped_cycles());
      c[GbaDebug::GuestPc]=compressed.cpu().state().regs[15];
      c[GbaDebug::Flags]=compressed.cpu().block_execution()?GbaDebug::BlocksOn:0u;
      if(compressed.cpu().alu_burst() && !compressed.cpu().block_execution())c[GbaDebug::Flags]|=GbaDebug::AluBurstOn;
      if(compressed.cpu().leaf_fetch())c[GbaDebug::Flags]|=GbaDebug::LeafFetchOn;
      if(compressed.cpu().fast_memory())c[GbaDebug::Flags]|=GbaDebug::FastMemoryOn;
      if(compressed.cpu().native_execution())c[GbaDebug::Flags]|=GbaDebug::NativeOn;
      c[GbaDebug::Keys]=uint32_t((~compressed.bus().keyinput())&0x03ffu);
      c[GbaDebug::KeyReads]=compressed.bus().keyinput_read_count();return c;
    };
    auto previousLogCounters=binaryLogPath?captureLogCounters(0):GbaDebug::Counters{};
    const unsigned frames=getenv("GBA_TEST_FRAMES")?unsigned(std::stoul(getenv("GBA_TEST_FRAMES"))):1200;
    for(unsigned frame=0;frame<frames;++frame) {
      if(directVideo && frame==3000) {
        // Model HDMI restart: drain/detach, preserve pixels, retire old storage,
        // restore the allocation and bind it before submitting another job.
        worker.set_video_target(nullptr,0);
        auto replacement=videoPixels;
        worker.set_video_target(replacement.data()+48*320+40,320);
        videoPixels.swap(replacement);
      }
      uint16_t keys=0;
      if(frame>=300 && frame<305)keys=1; // A: select English.
      if(frame>=800 && frame<805)keys=1<<3; // Start: exercise title input.
      if(getenv("GBA_TEST_AUTO_INPUT")) {
        if(frame>=1200 && frame%300<8)keys|=1;
        if(frame>=2400 && frame<2408)keys=1<<3;
        if(frame>=3600)keys|=1<<4;
      }
      if(const char* policy=getenv("GBA_TEST_FRAME_POLICY"))
        keys=FrameBenchmark::keys(frame,unsigned(std::stoul(policy)));
      memory.set_keys(keys);compressed.set_keys(keys);
      if(getenv("GBA_TEST_SLICED")) {
        // Match the firmware's UART polling boundaries as well as run_frame().
        while(!memory.ppu().frame_ready()) {
          const auto target=memory.cpu().current_cycle()+1024;
          memory.run_until(target);compressed.run_until(target);
          check(memory.cpu().current_cycle()==compressed.cpu().current_cycle(),"Sliced cycle parity");
        }
        check(compressed.ppu().consume_frame_ready(),"Sliced frame readiness parity");
        (void)memory.ppu().consume_frame_ready();
      } else {memory.run_frame();compressed.run_frame();}
      check(memory.cpu().state()==compressed.cpu().state(),"Full CPU/banked register/flag parity with idle acceleration");
      check(memory.cpu().current_cycle()==compressed.cpu().current_cycle(),"Exact cycle parity with idle acceleration");
      check(memory.bus().idle_bus_state()==compressed.bus().idle_bus_state(),"Bus/prefetch/latch parity with idle acceleration");
      compressed.flush_render();
      if(renderProfiling) {
        const auto nowProfile=worker.profile_snapshot();
        const auto delta=gba::renderProfileDelta(nowProfile,previousRenderProfile);
        check(gba::renderProfileConsistent(delta),"Per-frame renderer phase partition");
        check(nowProfile.total_us==compressed.render_wall_us(),"Profile total equals Draw counter");
        check(nowProfile.dirty_jobs==worker.completed(),"Profile dirty jobs equal completed scanlines");
        previousRenderProfile=nowProfile;
      }
      if(binaryLogPath) {
        const auto counters=captureLogCounters(frame+1);
        auto delta=GbaDebug::frameDelta(counters,previousLogCounters);previousLogCounters=counters;
        check(GbaDebug::consistent(delta),"Binary per-frame timing partition");
        delta[GbaDebug::Flags]|=GbaDebug::TimingValid;
        check(recorder.append(1,delta),"Append binary frame");
        if(frame%300==299) {
          GbaDebug::Profile pc{};pc[GbaDebug::PcFrame]=frame+1;
          pc[GbaDebug::PcId1]=pc[GbaDebug::PcId2]=pc[GbaDebug::PcId3]=0xffffu;
          check(recorder.append(2,pc),"Append periodic binary profile");
        }
      }
      if(directVideo) {
        const auto expected=memory.framebuffer();
        for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x) {
          uint16_t color=0xdead;
          if(y>=48 && y<208 && x>=40 && x<280) {
            const uint16_t v=expected[(y-48)*240+x-40];
            color=uint16_t(((v&31)<<11)|(((v>>5)&31)<<6)|((v>>10)&31));
          }
          check(videoPixels[y*320+x]==color,"Direct RGB565 pixels and protected margins match ordinary framebuffer path");
        }
      } else check(hashFrame(memory.framebuffer())==hashFrame(compressed.framebuffer()),"Framebuffer parity");
      check(std::equal(memory.bus().ewram().begin(),memory.bus().ewram().end(),compressed.bus().ewram().begin()),"EWRAM parity");
      check(std::equal(memory.bus().iwram().begin(),memory.bus().iwram().end(),compressed.bus().iwram().begin()),"IWRAM parity");
      check(!stats->failed,"Runtime decompression");
      if(frame%300==299) {
        std::cout<<"Frame "<<frame+1<<" PC "<<std::hex<<compressed.cpu().state().regs[15]<<" DISPCNT "<<compressed.ppu().dispcnt()<<" HASH "<<hashFrame(directVideo?memory.framebuffer():compressed.framebuffer())<<std::dec<<" KEYREADS "<<compressed.bus().keyinput_read_count()<<std::endl;
        if(const char* dir=getenv("GBA_TEST_CAPTURE_DIR"))
          writeFrame(std::string(dir)+"/frame-"+std::to_string(frame+1)+".ppm",directVideo?memory.framebuffer():compressed.framebuffer());
      }
    }
    std::cout<<"PASS: compressed vs original ROM CPU/frame parity for "<<frames<<" emulated frames with input\n";
    if(binaryLogPath) {
      GbaDebug::Config config;config.cpuHz=config.psramHz=0; // Host fixture, not board performance.
      check(recorder.startDump(config),"Freeze native log export");
      std::ofstream logFile(binaryLogPath,std::ios::binary);unsigned part=0;
      while(recorder.exporting()) {
        const auto pending=recorder.pending();const size_t n=std::min(pending.size(),size_t(1+(part++*17)%64));
        logFile.write(reinterpret_cast<const char*>(pending.data()),n);recorder.consume(n);
      }
      check(bool(logFile),"Save native binary log");
      std::cout<<"PASS: binary frame/profile logging with complete-record eviction and partial-write export; bytes "
          <<recorder.used()<<" records "<<recorder.count()<<" overwritten "<<recorder.dropped()<<"\n";
    }
    if(directVideo)std::cout<<"PASS: direct RGB565 output and protected margins every frame, including display target replacement at frame 3000\n";
    if(threaded) {
      compressed.flush_render();check(worker.completed()>0,"Real render worker exercised");
      check(worker.wait_wall_us()==uint32_t(worker.full_wait_wall_us()+worker.barrier_wait_wall_us()),"Queue-full and barrier wait accounting sums to total modulo timer wrap");
      std::cout<<"PASS: threaded renderer completed "<<worker.completed()<<" scanlines\n";
      std::cout<<"PASS: queue-full and barrier elapsed counters sum to total wait\n";
      if(renderProfiling)std::cout<<"PASS: core-1 phase partition, Draw total and completed-job accounting on every frame\n";
    }
    std::cout<<"Idle cycles skipped "<<compressed.cpu().idle_skipped_cycles()<<" of "<<compressed.cpu().current_cycle()<<"\n";
    std::cout<<"VBlank polling cycles skipped "<<compressed.cpu().vblank_skipped_cycles()<<"\n";
    std::cout<<"Instruction fast reads "<<compressed.bus().code_fetch_fast_hits<<" of "<<compressed.bus().code_fetch_attempts<<"\n";
    std::cout<<"Thumb ALU fast executions "<<compressed.cpu().fast_thumb_alu_count<<"\n";
    if(compressed.cpu().native_execution()) {
      const auto s=compressed.cpu().native_stats();
      std::cout<<"Native model: compiled "<<s.compiled<<" batches "<<s.batches<<" instructions "<<s.instructions<<" exits "<<s.exits<<" mismatches "<<s.mismatches<<"\n";
      check(s.batches>0 && s.instructions>s.batches,"Game replay exercises mixed multi-instruction JIT plans");
    }
    std::cout<<"Instruction window hits "<<compressed.cartridge().instruction_hits()<<" fills "<<compressed.cartridge().instruction_fills()<<"\n";
    std::cout<<"Inline instruction fetch hits "<<compressed.bus().code_line_leaf_hits<<"\n";
    std::cout<<"Thumb base-in-list cases: loads "<<compressed.cpu().base_load_corrections<<" stores "<<compressed.cpu().base_store_corrections<<"\n";
    std::cout<<"Video writes "<<compressed.bus().video_writes<<" unchanged "<<compressed.bus().unchanged_video_writes
      <<" register halves "<<(compressed.ppu().register_writes+compressed.ppu().skipped_register_halves)
      <<" unchanged "<<(compressed.ppu().unchanged_register_writes+compressed.ppu().skipped_register_halves)<<"\n";
    std::cout<<"Display register halves bypassed before join "<<compressed.ppu().skipped_register_halves<<"\n";
    std::cout<<"Tile cache optimized hits "<<compressed.ppu().tile_hits<<" misses "<<compressed.ppu().tile_misses
      <<" reference hits "<<memory.ppu().tile_hits<<" misses "<<memory.ppu().tile_misses
      <<" partial invalidations "<<compressed.ppu().partial_invalidations<<"\n";
    // Individual ROMs need not contain the specific safely recognized RAM wait.
    // testRamWait() verifies both acceleration and wake-up independently.
    const uint64_t reads=uint64_t(stats->hotHits)+stats->hotMisses;
    std::cout<<"SRAM ROM cache: hits "<<stats->hotHits<<" misses "<<stats->hotMisses
      <<" hit percent "<<(reads?100.0*stats->hotHits/reads:0.0)
      <<" backing page misses "<<stats->misses<<"\n";
    writeFrame(argv[3],directVideo?memory.framebuffer():compressed.framebuffer());
    std::cout<<"Frame hash "<<std::hex<<hashFrame(directVideo?memory.framebuffer():compressed.framebuffer())<<" PC "<<compressed.cpu().state().regs[15]<<"\n";
    if(pcSampleCount) {
      std::vector<std::pair<uint32_t,PcSample>> sorted(pcSamples.begin(),pcSamples.end());
      std::sort(sorted.begin(),sorted.end(),[](auto& a,auto& b){return a.second.count>b.second.count;});
      std::cout<<std::dec<<"Executed instruction samples "<<pcSampleCount<<" (native guest-code distribution, not board wall time)\n";
      for(unsigned i=0;i<std::min(size_t(25),sorted.size());++i) {
        auto& [pc,s]=sorted[i];
        std::cout<<std::hex<<"PC "<<(pc&~15u)<<((pc&1)?" Thumb":" ARM")<<std::dec<<" samples "<<s.count<<" pct "<<100.0*s.count/pcSampleCount<<std::hex;
        for(unsigned r=0;r<4;++r)std::cout<<" r"<<r<<"="<<s.example.regs[r];
        std::cout<<"\n";
      }
    }
    return 0;
  } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
