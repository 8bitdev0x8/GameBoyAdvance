#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#include "DebugLog.h"
using namespace GbaDebug;
static void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
static std::vector<uint8_t> dump(Recorder& r) {
  Config config;config.mapFingerprint=123;config.dependentNs={21,767,1188};
  check(r.startDump(config),"Start export");
  Counters blocked{};check(!r.append(1,blocked) && !r.clear(),"Freeze appends/clear during export");
  std::vector<uint8_t> out;
  size_t sequence=0;
  while(r.exporting()) {
    auto bytes=r.pending();check(!bytes.empty(),"Export makes progress");
    if((sequence%7)==0) {r.consume(0);check(r.pending().size()==bytes.size(),"Zero write preserves data");}
    const size_t count=std::min(bytes.size(),size_t(1+(sequence++*17)%61));
    out.insert(out.end(),bytes.begin(),bytes.begin()+count);r.consume(count);
  }
  check(out.size()==HeaderBytes+r.used()+8,"Exact export length");
  check(crc32({out.data(),60})==(uint32_t(out[60])|(uint32_t(out[61])<<8)|(uint32_t(out[62])<<16)|(uint32_t(out[63])<<24)),"Header CRC");
  check(std::equal(std::begin(End),std::end(End),out.end()-8),"Trailer");
  return out;
}
int main(int argc,char** argv) {
  try {
    const char* known="123456789";
    check(crc16({reinterpret_cast<const uint8_t*>(known),9})==0x29b1,"CRC16 standard vector");
    check(crc32({reinterpret_cast<const uint8_t*>(known),9})==0xcbf43926,"CRC32 standard vector");
    Counters before{},now{};
    before[RunUs]=0xfffffff0u;now[RunUs]=14;
    before[WallUs]=0xfffffff0u;now[WallUs]=84;
    now[GuestPc]=0x08001234;now[FrameNo]=33;now[Keys]=0x101;
    const auto delta=frameDelta(now,before);
    check(delta[RunUs]==30 && delta[WallUs]==100 && delta[FrameNo]==33 && delta[GuestPc]==0x08001234 && delta[Keys]==0x101,"Counter wrap and absolute fields");
    Counters valid{};valid[DrawUs]=valid[WorkerUs]=100;
    valid[PrepUs]=20;valid[MixUs]=80;valid[WaitUs]=10;valid[QfullUs]=9;valid[BarrierUs]=1;
    check(consistent(valid),"Valid phase/wait partitions");valid[BarrierUs]=2;
    check(!consistent(valid),"Invalid partition detected");
    std::vector<uint8_t> storage(1024);Recorder recorder;recorder.bind(storage);
    auto empty=dump(recorder);check(empty.size()==72,"Empty download");
    check(recorder.startDump({}),"Second export");recorder.cancel();check(!recorder.exporting(),"Cancel export");
    std::mt19937 rng(41);
    std::vector<Counters> expected;
    for(unsigned frame=1;frame<=400;++frame) {
      Counters sample{};sample[FrameNo]=frame;
      for(unsigned i=1;i<FieldCount;++i)if(rng()%4)sample[i]=rng();
      if(frame==400)sample.fill(0xffffffffu);
      // Preserve a monotonic frame identifier while exercising maximum varints.
      sample[FrameNo]=frame;expected.push_back(sample);
      check(recorder.append(1,sample),"Append sparse/max frame record");
    }
    check(recorder.dropped()>0 && recorder.count()>0 && recorder.used()<=storage.size(),"Ring evicts complete records");
    auto data=dump(recorder);size_t offset=HeaderBytes,records=0;
    const size_t first=expected.size()-recorder.count();
    while(offset<data.size()-8) {
      const size_t length=read16(data.data()+offset);
      check(length>=9 && length<=MaxRecordBytes && offset+length<=data.size()-8,"Record bounds");
      check(crc16({data.data()+offset,length-2})==read16(data.data()+offset+length-2),"Wrapped record CRC");
      check(data[offset+2]==1,"Frame type");
      uint32_t mask=0;for(unsigned i=0;i<4;++i)mask|=uint32_t(data[offset+3+i])<<(8*i);
      Counters actual{};size_t at=offset+7;
      for(unsigned i=0;i<FieldCount;++i)if(mask&(1u<<i)) {
        unsigned shift=0;uint8_t b;
        do {check(at<offset+length-2 && shift<35,"Varint bounds");b=data[at++];actual[i]|=uint32_t(b&127u)<<shift;shift+=7;}while(b&128);
      }
      check(at==offset+length-2 && actual==expected[first+records],"Lossless chronological ring decode");
      offset+=length;++records;
    }
    check(records==recorder.count(),"Record count");
    recorder.clear();check(recorder.used()==0 && recorder.count()==0 && recorder.dropped()==0,"Clear ring");
    for(unsigned count:{0u,1u,6u,12u,13u}) {
      HistogramWriter writer;writer.begin(1,5000,100,0);
      for(unsigned i=0;i<count;++i)writer.add(recorder,i,i+1);
      writer.finish(recorder);
      check(recorder.count()==count/6+1,"Histogram boundaries retain an end marker, including empty/full final chunks");
      recorder.clear();
    }
    Counters frame{};frame[FrameNo]=7;frame[UptimeMs]=700;frame[WallUs]=100000;
    frame[RunUs]=53000;frame[DrawUs]=frame[WorkerUs]=18300;
    frame[BgUs]=2600;frame[ObjUs]=800;frame[MixUs]=11600;
    frame[PrepUs]=1900;frame[RgbUs]=1200;frame[OtherUs]=200;
    frame[WaitUs]=8000;frame[QfullUs]=7900;frame[BarrierUs]=100;frame[Flags]=TimingValid|PadOn;
    frame[Jobs]=frame[DirtyJobs]=160;frame[GuestPc]=0x08001234;
    check(consistent(frame),"Cross-language fixture partition");
    check(recorder.append(1,frame),"Fixture frame");
    Profile profile{};profile[PcFrame]=7;profile[PcSamples]=2500;profile[PcWindowMs]=5000;
    profile[PcFlags]=1;profile[PcId1]=profile[PcId2]=profile[PcId3]=0xffff;
    check(recorder.append(2,profile),"Fixture profile");
    HistogramWriter histogram;
    histogram.begin(7,5000,100,0);
    for(unsigned i=0;i<13;++i)histogram.add(recorder,400+i,i+1);
    histogram.finish(recorder);
    check(recorder.count()==5,"Thirteen histogram functions use three complete-record chunks");
    auto fixture=dump(recorder);
    if(argc==2) {std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(fixture.data()),fixture.size());check(bool(file),"Write protocol fixture");}
    std::cout<<"PASS: binary CRC vectors, counter wrap, sparse/max varints, bounded ring eviction and lossless chronological decode\n";
    std::cout<<"PASS: empty/partial/zero-byte USB export, frozen appends, cancellation, clear and cross-language frame/profile fixture\n";
  } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
