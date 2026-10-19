#pragma once
#include <stdint.h>
#include <string.h>
struct ControllerState {
  bool connected=false;
  uint8_t raw[78]={};
  enum Button : uint32_t {
    SQUARE=1UL<<4,CROSS=1UL<<5,CIRCLE=1UL<<6,TRIANGLE=1UL<<7,
    L1=1UL<<8,R1=1UL<<9,L2=1UL<<10,R2=1UL<<11,
    CREATE=1UL<<12,OPTIONS=1UL<<13,L3=1UL<<14,R3=1UL<<15,
    PS=1UL<<16,TOUCH_CLICK=1UL<<17,MUTE=1UL<<18
  };
  void clear() {
    connected=false; memset(raw,0,sizeof(raw));
    raw[2]=raw[3]=raw[4]=raw[5]=128;
    raw[9]=8; raw[34]=raw[38]=128;
  }
  int leftX() const { return int(raw[2])-128; }
  int leftY() const { return int(raw[3])-128; }
  int rightX() const { return int(raw[4])-128; }
  int rightY() const { return int(raw[5])-128; }
  uint8_t l2() const { return raw[6]; }
  uint8_t r2() const { return raw[7]; }
  uint8_t dpad() const { return raw[9]&15; }
  bool up() const { return dpad()==0||dpad()==1||dpad()==7; }
  bool right() const { return dpad()==1||dpad()==2||dpad()==3; }
  bool down() const { return dpad()==3||dpad()==4||dpad()==5; }
  bool left() const { return dpad()==5||dpad()==6||dpad()==7; }
  bool button(Button b) const {
    return (uint32_t(raw[9])|(uint32_t(raw[10])<<8)|(uint32_t(raw[11])<<16))&b;
  }
  int16_t signed16(unsigned o) const {
    return int16_t(uint16_t(raw[o])|(uint16_t(raw[o+1])<<8));
  }
  int16_t gyroX() const { return signed16(17); }
  int16_t gyroY() const { return signed16(19); }
  int16_t gyroZ() const { return signed16(21); }
  int16_t accelX() const { return signed16(23); }
  int16_t accelY() const { return signed16(25); }
  int16_t accelZ() const { return signed16(27); }
  uint32_t sensorTimestamp() const {
    return uint32_t(raw[29])|(uint32_t(raw[30])<<8)|
      (uint32_t(raw[31])<<16)|(uint32_t(raw[32])<<24);
  }
  bool touchActive(unsigned i) const { return i<2 && !(raw[34+4*i]&128); }
  uint8_t touchId(unsigned i) const { return i<2?raw[34+4*i]&127:0; }
  uint16_t touchX(unsigned i) const {
    if(i>=2) return 0;
    unsigned o=34+4*i;
    return uint16_t(raw[o+1])|(uint16_t(raw[o+2]&15)<<8);
  }
  uint16_t touchY(unsigned i) const {
    if(i>=2) return 0;
    unsigned o=34+4*i;
    return (raw[o+2]>>4)|(uint16_t(raw[o+3])<<4);
  }
  uint8_t batteryLevelRaw() const { return raw[54]&15; }
  uint8_t chargeStateRaw() const { return raw[54]>>4; }
};
