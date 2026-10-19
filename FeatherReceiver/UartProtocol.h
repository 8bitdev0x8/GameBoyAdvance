#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace PadWire {
constexpr size_t REPORT_SIZE=78, PACKET_SIZE=84, FRAME_SIZE=86;
inline uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t crc=0xffff;
  while(n--) {
    crc ^= uint16_t(*p++)<<8;
    for(uint8_t i=0;i<8;++i)
      crc=(crc&0x8000)?uint16_t((crc<<1)^0x1021):uint16_t(crc<<1);
  }
  return crc;
}
inline size_t encode(const uint8_t* report,bool online,uint16_t sequence,uint8_t* out) {
  uint8_t p[PACKET_SIZE]={};
  p[0]=1; p[1]=online?1:0; p[2]=uint8_t(sequence); p[3]=uint8_t(sequence>>8);
  if(online) memcpy(p+4,report,REPORT_SIZE);
  const uint16_t crc=crc16(p,82);
  p[82]=uint8_t(crc); p[83]=uint8_t(crc>>8);
  size_t codeIndex=0,used=1; uint8_t code=1;
  for(size_t i=0;i<PACKET_SIZE;++i) {
    if(p[i]) { out[used++]=p[i]; ++code; }
    else { out[codeIndex]=code; codeIndex=used++; code=1; }
  }
  out[codeIndex]=code; out[used++]=0;
  return used;
}
// Input excludes zero delimiter. Bounds checked before every decoded write.
inline bool decode(const uint8_t* in,size_t length,uint8_t* p) {
  if(!length || length>=FRAME_SIZE) return false;
  size_t read=0,used=0;
  while(read<length) {
    const uint8_t code=in[read++];
    if(!code || read+code-1>length) return false;
    for(unsigned i=1;i<code;++i) {
      if(used>=PACKET_SIZE || !in[read]) return false;
      p[used++]=in[read++];
    }
    if(code!=255 && read<length) {
      if(used>=PACKET_SIZE) return false;
      p[used++]=0;
    }
  }
  if(used!=PACKET_SIZE || p[0]!=1 || p[1]>1) return false;
  if(p[1] && p[4]!=0x31) return false;
  return crc16(p,82)==(uint16_t(p[82])|(uint16_t(p[83])<<8));
}
struct Parser {
  uint8_t buffer[FRAME_SIZE-1]={};
  size_t used=0; bool overflow=false;
  bool push(uint8_t b,uint8_t* packet) {
    if(b) {
      if(used<sizeof(buffer)) buffer[used++]=b;
      else overflow=true;
      return false;
    }
    const bool valid=!overflow && decode(buffer,used,packet);
    used=0; overflow=false;
    return valid;
  }
};
}
