#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include "CompressedRom.h"
#include "gba/core/emulator.hpp"
#include "gba/core/render_worker.hpp"

const uint8_t* gba_rom_packed=nullptr;
std::vector<uint8_t> load(const char* path){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Missing probe input");return {std::istreambuf_iterator<char>(file),{}};}
uint32_t checksum(std::span<const uint8_t> bytes){uint32_t hash=2166136261u;for(auto value:bytes)hash=(hash^value)*16777619u;return hash;}
struct InlineRenderer {
    std::array<uint16_t,240> row{};
    uint16_t* target=nullptr;
    uint32_t completed=0;
    static void wait(void*){}
    static void submit(void* context,gba::Ppu& ppu,int line,std::span<const uint8_t> vram,
                       std::span<const uint8_t> palette,std::span<const uint8_t> oam,uint32_t* elapsed) {
        auto& self=*static_cast<InlineRenderer*>(context);
        if(!ppu.is_dirty(line))return;
        const auto started=micros();
        ppu.render_scanline(line,vram,palette,oam,self.row.data());
        for(unsigned x=0;x<240;++x){const auto v=self.row[x];self.target[line*320+x]=uint16_t(((v&31)<<11)|(((v>>5)&31)<<6)|((v>>10)&31));}
        *elapsed+=uint32_t(micros()-started);++self.completed;
    }
};
struct WorkerThread {
    gba::ScanlineWorker& worker;
    std::atomic<bool> stop{false};
    std::thread thread;
    WorkerThread(gba::ScanlineWorker& value,bool enabled):worker(value){if(enabled)thread=std::thread([this]{while(!stop.load())if(!worker.run_one())std::this_thread::yield();});}
    ~WorkerThread(){if(thread.joinable()){worker.wait();stop.store(true);thread.join();}}
};
int main(int argc,char** argv) {
    try {
        if(argc!=6)throw std::runtime_error("Usage: probe packed.bin bios.bin thread|inline|compute warmup frames");
        auto packed=load(argv[1]);auto bios=load(argv[2]);gba_rom_packed=packed.data();
        const std::string mode=argv[3];const unsigned warmup=std::stoul(argv[4]),frames=std::stoul(argv[5]);
        if(mode!="thread" && mode!="inline" && mode!="compute")throw std::runtime_error("Invalid mode");
        gba::Emulator emulator;auto provider=std::make_unique<CompressedRom>();auto* cache=provider.get();
        emulator.set_rom_provider(std::move(provider));emulator.cartridge().auto_detect_save_type();
        emulator.load_bios(std::move(bios));emulator.reset(false);emulator.apu().set_synthesis_enabled(false);
        emulator.cpu().set_idle_acceleration(true);emulator.cpu().set_fast_fetch(true);
#ifdef GBA_RUNTIME_MEMORY_OFF
        emulator.cpu().set_fast_memory(false);
#endif
#ifdef GBA_DISABLE_LEAF_FETCH
        emulator.cpu().set_leaf_fetch(false);
#endif
#ifdef GBA_FORCE_INTERPRETER_DEFAULT
        emulator.cpu().set_alu_burst(false);
        emulator.cpu().set_block_execution(false);
#else
#ifdef GBA_DISABLE_ALU_BURST
        emulator.cpu().set_alu_burst(false);
#else
        emulator.cpu().set_alu_burst(true);
#endif
#ifndef GBA_DISABLE_BLOCKS
        emulator.cpu().set_block_execution(true);
#endif
#endif
        std::vector<uint16_t> video(320*240,0xdead);
        for(unsigned y=48;y<208;++y)std::fill_n(video.data()+y*320+40,240,0xffdf);
        gba::ScanlineWorker worker;
        InlineRenderer inlineRenderer;inlineRenderer.target=video.data()+48*320+40;
        WorkerThread rendererThread(worker,mode=="thread");
        if(mode=="thread"){worker.set_video_target(video.data()+48*320+40,320);emulator.set_render_backend(worker.backend());}
        if(mode=="inline")emulator.set_render_backend({&inlineRenderer,&InlineRenderer::submit,&InlineRenderer::wait});
        if(mode=="compute")emulator.set_skip_render(true);
        auto frame=[&](unsigned index) {
            uint16_t keys=0;
            if(index>=300 && index<305)keys=1;
            if(index>=800 && index<805)keys=1<<3;
            if(index>=1200 && index%300<8)keys|=1;
            if(index>=2400 && index<2408)keys=1<<3;
            if(index>=3600)keys|=1<<4;
            emulator.set_keys(keys);
            do{emulator.run_until(emulator.cpu().current_cycle()+1024);}while(!emulator.consume_frame_ready());
            if(cache->failed)throw std::runtime_error("ROM decode failure");
        };
        for(unsigned index=0;index<warmup;++index)frame(index);
        const uint32_t drawBefore=emulator.render_wall_us(),waitBefore=worker.wait_wall_us();
        const auto submittedBefore=worker.submitted,cleanBefore=worker.clean_jobs,
            fullBefore=worker.full_waits,barrierBefore=worker.barrier_waits;
        const auto fullUsBefore=worker.full_wait_us,barrierUsBefore=worker.barrier_wait_us;
        const auto start=std::chrono::steady_clock::now();
        for(unsigned index=0;index<frames;++index)frame(warmup+index);
        emulator.flush_render();
        const double wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        uint32_t cpuHash=checksum(emulator.bus().ewram());cpuHash^=checksum(emulator.bus().iwram());
        for(auto value:emulator.cpu().state().regs)cpuHash=(cpuHash^value)*16777619u;
        cpuHash=(cpuHash^emulator.cpu().state().cpsr)*16777619u;
        uint32_t videoHash=2166136261u;for(auto value:video){videoHash=(videoHash^uint8_t(value))*16777619u;videoHash=(videoHash^uint8_t(value>>8))*16777619u;}
        std::cout<<std::fixed<<std::setprecision(3)<<"{\"mode\":\""<<mode<<"\",\"warmup\":"<<warmup<<",\"frames\":"<<frames
          <<",\"wall_ms\":"<<wall<<",\"host_fps\":"<<frames*1000.0/wall
          <<",\"draw_ms\":"<<uint32_t(emulator.render_wall_us()-drawBefore)/1000.0
          <<",\"wait_ms\":"<<uint32_t(worker.wait_wall_us()-waitBefore)/1000.0
          <<",\"queued_jobs\":"<<worker.submitted-submittedBefore<<",\"clean_jobs\":"<<worker.clean_jobs-cleanBefore
          <<",\"full_waits\":"<<worker.full_waits-fullBefore<<",\"barrier_waits\":"<<worker.barrier_waits-barrierBefore
          <<",\"full_wait_ms\":"<<uint32_t(worker.full_wait_us-fullUsBefore)/1000.0
          <<",\"barrier_wait_ms\":"<<uint32_t(worker.barrier_wait_us-barrierUsBefore)/1000.0
          <<",\"max_queued\":"<<worker.max_queued
          <<",\"cycles\":"<<emulator.cpu().current_cycle()<<",\"pc\":"<<emulator.cpu().state().regs[15]
          <<",\"cpu_hash\":"<<cpuHash<<",\"video_hash\":"<<videoHash
          <<",\"block_lookups\":"<<emulator.cpu().block_stats().lookups
          <<",\"block_hits\":"<<emulator.cpu().block_stats().hits
          <<",\"block_misses\":"<<emulator.cpu().block_stats().misses
          <<",\"block_batches\":"<<emulator.cpu().block_stats().batches
          <<",\"block_instructions\":"<<emulator.cpu().block_stats().instructions
          <<",\"block_mismatches\":"<<emulator.cpu().block_stats().mismatches
          <<",\"burst_instructions\":"<<emulator.cpu().burst_instructions
          <<",\"burst_entries\":"<<emulator.cpu().burst_exits
          <<",\"burst_enabled\":"<<(emulator.cpu().alu_burst()?1:0)
          <<",\"leaf_fetch_enabled\":"<<(emulator.cpu().leaf_fetch()?1:0)
          <<",\"leaf_fetch_hits\":"<<emulator.bus().code_line_leaf_hits<<"}\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
