#ifndef RP2350_GBA_DEBUG_LOG_H
#define RP2350_GBA_DEBUG_LOG_H
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace GbaDebug {
// Format v1: one sparse u32 frame record, plus periodic CPU sampling records.
// Scalars are little endian; nonzero fields are unsigned LEB128, in mask order.
enum Field : size_t {
  FrameNo,UptimeMs,WallUs,RunUs,DrawUs,CopyUs,WaitUs,QfullUs,BarrierUs,
  PrepUs,BgUs,ObjUs,MixUs,RgbUs,OtherUs,WorkerUs,Jobs,DirtyJobs,
  RomMisses,DecodeUs,IFetchHits,IFetchFills,GuestCycles,VblankCycles,
  HdmiIrqs,HdmiIrqUs,GuestPc,Flags,Keys,HdmiResets,FreeHeap,KeyReads,FieldCount
};
static_assert(FieldCount==32);
enum ProfileField : size_t {
  PcFrame,PcUptimeMs,PcWindowMs,PcSamples,PcBad,PcUnmapped,PcHandlerUs,
  PcIrqSamples,PcFlags,PcId1,PcPercent1,PcId2,PcPercent2,PcId3,PcPercent3,PcFreeHeap,ProfileFieldCount
};
// Type 3: exact full native-PC histogram, six ID/count pairs per chunk.
// A window's chunks share frame/window/samples/flags. Zero-count pairs are padding.
enum HistogramField : size_t {HistFrame,HistWindowMs,HistSamples,HistFlags,HistId1,HistHits1};
enum Flag : uint32_t {PadOn=1,BiosSram=2,TimingValid=4,UsbAttached=16,BlocksOn=32,AluBurstOn=64,LeafFetchOn=128,FastMemoryOn=256,NativeOn=512,HstxDmaRing=1024};
using Counters=std::array<uint32_t,FieldCount>;
using Profile=std::array<uint32_t,ProfileFieldCount>;
constexpr size_t StorageBytes=24*1024,HeaderBytes=64,MaxRecordBytes=169;
inline constexpr uint8_t Magic[8]={'G','B','A','L','O','G','4','1'};
inline constexpr uint8_t End[8]={'G','B','A','E','N','D','4','1'};
inline uint16_t read16(const uint8_t* p) {return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline void put16(uint8_t* p,uint16_t n) {p[0]=uint8_t(n);p[1]=uint8_t(n>>8);}
inline void put32(uint8_t* p,uint32_t n) {for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(i*8));}
inline uint16_t crc16(std::span<const uint8_t> bytes) {
  uint16_t crc=0xffff;
  for(uint8_t b:bytes) {
    crc^=uint16_t(b)<<8;
    for(unsigned i=0;i<8;++i)crc=uint16_t((crc<<1)^((crc&0x8000)?0x1021:0));
  }
  return crc;
}
inline uint32_t crc32(std::span<const uint8_t> bytes) {
  uint32_t crc=0xffffffffu;
  for(uint8_t b:bytes) {
    crc^=b;
    for(unsigned i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);
  }
  return ~crc;
}
inline uint32_t fingerprint(std::span<const uint8_t> bytes) {
  uint32_t hash=2166136261u;
  for(uint8_t b:bytes)hash=(hash^b)*16777619u;
  return hash;
}
inline Counters frameDelta(const Counters& now,const Counters& before) {
  // Absolute values remain absolute; clocks/cumulative counters subtract modulo 2^32.
  constexpr uint32_t absolute=(1u<<FrameNo)|(1u<<UptimeMs)|(1u<<GuestPc)|
      (1u<<Flags)|(1u<<Keys)|(1u<<HdmiResets)|(1u<<FreeHeap);
  Counters delta=now;
  for(unsigned i=0;i<FieldCount;++i)if(!(absolute&(1u<<i)))delta[i]-=before[i];
  return delta;
}
inline bool consistent(const Counters& delta) {
  const uint64_t phases=uint64_t(delta[PrepUs])+delta[BgUs]+delta[ObjUs]+delta[MixUs]+delta[RgbUs]+delta[OtherUs];
  return phases==delta[WorkerUs] && delta[WorkerUs]==delta[DrawUs] &&
      uint64_t(delta[QfullUs])+delta[BarrierUs]==delta[WaitUs] && delta[DirtyJobs]<=delta[Jobs];
}
struct Config {
  uint32_t cpuHz=240000000,psramHz=30000000,mapFingerprint=0;
  std::array<uint32_t,3> dependentNs{};
  uint32_t flags=0,uptimeMs=0;
};

// Core 0 owns all ring indices. Appends freeze during export. Each record has
// u16 total size, u8 type, u32 nonzero mask, varints, then CRC16 over all prior bytes.
class Recorder {
  std::span<uint8_t> storage_{};
  size_t head_=0,used_=0,count_=0,cursor_=0,pending_=0;
  uint32_t dropped_=0;
  bool exporting_=false;
  std::array<uint8_t,MaxRecordBytes> scratch_{};
  std::array<uint8_t,HeaderBytes> header_{};
  std::array<uint8_t,64> transfer_{};
  uint8_t ringByte(size_t offset) const {return storage_[(head_+offset)%storage_.size()];}
public:
  void bind(std::span<uint8_t> storage) {cancel();storage_=storage;clear();}
  size_t count() const {return count_;}
  size_t used() const {return used_;}
  uint32_t dropped() const {return dropped_;}
  bool available() const {return storage_.size()>=MaxRecordBytes;}
  bool exporting() const {return exporting_;}
  bool clear() {
    if(exporting_)return false;
    head_=used_=count_=0;dropped_=0;return true;
  }
  bool append(uint8_t type,std::span<const uint32_t> values) {
    if(!available() || exporting_ || !((type==1 && values.size()==FieldCount) ||
       ((type==2 || type==3) && values.size()==ProfileFieldCount)))return false;
    uint32_t mask=0;
    for(unsigned i=0;i<values.size();++i)if(values[i])mask|=1u<<i;
    scratch_[2]=type;put32(scratch_.data()+3,mask);size_t length=7;
    for(uint32_t n:values)if(n)do {
      const uint8_t b=uint8_t(n&127u);n>>=7;
      scratch_[length++]=uint8_t(b|(n?128u:0u));
    }while(n);
    put16(scratch_.data(),uint16_t(length+2));
    put16(scratch_.data()+length,crc16({scratch_.data(),length}));length+=2;
    while(storage_.size()-used_<length) {
      const size_t oldest=size_t(ringByte(0))|(size_t(ringByte(1))<<8);
      if(oldest<9 || oldest>MaxRecordBytes || oldest>used_) {clear();break;}
      head_=(head_+oldest)%storage_.size();used_-=oldest;--count_;++dropped_;
    }
    const size_t at=(head_+used_)%storage_.size(),first=std::min(length,storage_.size()-at);
    std::memcpy(storage_.data()+at,scratch_.data(),first);
    std::memcpy(storage_.data(),scratch_.data()+first,length-first);
    used_+=length;++count_;return true;
  }
  bool startDump(const Config& config) {
    if(exporting_)return false;
    std::memcpy(header_.data(),Magic,8);
    put16(header_.data()+8,1);put16(header_.data()+10,HeaderBytes);
    put16(header_.data()+12,FieldCount);put16(header_.data()+14,ProfileFieldCount);
    const uint32_t fields[]={uint32_t(used_),uint32_t(count_),dropped_,config.cpuHz,config.psramHz,
      config.mapFingerprint,config.dependentNs[0],config.dependentNs[1],config.dependentNs[2],config.flags,config.uptimeMs};
    for(unsigned i=0;i<11;++i)put32(header_.data()+16+4*i,fields[i]);
    put32(header_.data()+60,crc32({header_.data(),60}));
    cursor_=pending_=0;exporting_=true;return true;
  }
  void cancel() {exporting_=false;cursor_=pending_=0;}
  std::span<const uint8_t> pending() {
    if(!exporting_)return {};
    if(cursor_<HeaderBytes) {
      pending_=HeaderBytes-cursor_;return {header_.data()+cursor_,pending_};
    }
    if(cursor_<HeaderBytes+used_) {
      const size_t offset=cursor_-HeaderBytes;
      pending_=std::min(used_-offset,transfer_.size());
      const size_t at=(head_+offset)%storage_.size(),first=std::min(pending_,storage_.size()-at);
      std::memcpy(transfer_.data(),storage_.data()+at,first);
      std::memcpy(transfer_.data()+first,storage_.data(),pending_-first);
      return {transfer_.data(),pending_};
    }
    const size_t offset=cursor_-HeaderBytes-used_;
    pending_=sizeof(End)-offset;return {End+offset,pending_};
  }
  void consume(size_t bytes) {
    if(!exporting_ || bytes>pending_)return;
    cursor_+=bytes;pending_=0;
    if(cursor_==HeaderBytes+used_+sizeof(End))exporting_=false;
  }
};
class HistogramWriter {
  Profile chunk_{};
  unsigned pairs_=0;
  void flush(Recorder& recorder,bool last) {
    if(last)chunk_[HistFlags]|=8u;
    recorder.append(3,chunk_);
    chunk_[HistFlags]&=~12u;
    for(unsigned n=4;n<16;++n)chunk_[n]=0;
    pairs_=0;
  }
public:
  void begin(uint32_t frame,uint32_t windowMs,uint32_t samples,uint32_t flags) {
    chunk_.fill(0);pairs_=0;
    chunk_[HistFrame]=frame;chunk_[HistWindowMs]=windowMs;
    chunk_[HistSamples]=samples;chunk_[HistFlags]=flags|4u;
  }
  void add(Recorder& recorder,unsigned index,unsigned count) {
    if(!count)return;
    chunk_[4+pairs_*2]=index;chunk_[5+pairs_*2]=count;
    if(++pairs_==6)flush(recorder,false);
  }
  // Always emit an end marker, even when the last data chunk was exactly full.
  void finish(Recorder& recorder) {flush(recorder,true);}
};
}
#endif
