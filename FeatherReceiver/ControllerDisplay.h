#pragma once
#include <Adafruit_dvhstx.h>
#include "ControllerState.h"

// Same video timing and pin mapping as the verified HDMITest sketch.
// Two RGB565 buffers (307200 bytes total) prevent visible redraw flicker.
DVHSTX16 display(ADAFRUIT_FEATHER_RP2350_CFG,DVHSTX_RESOLUTION_320x240,true);
namespace PadView {
constexpr uint16_t BG=0x0843, PANEL=0x18e6, EDGE=0x52ab, TEXT=0xe75f;
constexpr uint16_t MUTED=0x8c71, GREEN=0x47f4, RED=0xfacb, BLUE=0x5dff, AMBER=0xfdc8;
inline void label(int x,int y,const char* s,uint16_t color=TEXT) {
  display.setTextColor(color); display.setCursor(x,y); display.print(s);
}
inline void key(int x,int y,int w,const char* text,bool held,uint16_t accent=GREEN) {
  display.fillRoundRect(x,y,w,13,3,held?accent:BG);
  display.drawRoundRect(x,y,w,13,3,held?accent:EDGE);
  label(x+(w-int(strlen(text))*6)/2,y+3,text,held?BG:TEXT);
}
inline void face(int x,int y,char symbol,bool held,uint16_t color) {
  display.fillCircle(x,y,10,held?color:BG);
  display.drawCircle(x,y,10,color);
  const uint16_t ink=held?BG:color;
  if(symbol=='X') { display.drawLine(x-4,y-4,x+4,y+4,ink);display.drawLine(x-4,y+4,x+4,y-4,ink); }
  else if(symbol=='O') display.drawCircle(x,y,5,ink);
  else if(symbol=='S') display.drawRect(x-4,y-4,9,9,ink);
  else display.drawTriangle(x,y-5,x-5,y+4,x+5,y+4,ink);
}
inline void stick(int cx,int cy,int x,int y,bool held) {
  display.fillCircle(cx,cy,20,BG);display.drawCircle(cx,cy,20,held?GREEN:EDGE);
  display.drawFastHLine(cx-16,cy,33,PANEL);display.drawFastVLine(cx,cy-16,33,PANEL);
  display.fillCircle(cx+x*14/128,cy+y*14/128,5,held?GREEN:BLUE);
}
inline void trigger(int x,const char* name,uint8_t value,bool pressed) {
  label(x,25,name,pressed?GREEN:MUTED);
  display.drawRect(x+18,23,77,10,EDGE);
  display.fillRect(x+19,24,uint16_t(value)*75/255,8,GREEN);
  display.setCursor(x+99,25);display.print(value);
}
}

inline void beginControllerDisplay() {
  pinMode(LED_BUILTIN,OUTPUT);
  if(!display.begin()) {
    while(true) { digitalWrite(LED_BUILTIN,HIGH);delay(100);digitalWrite(LED_BUILTIN,LOW);delay(100); }
  }
  display.setTextWrap(false);
}

inline void drawControllerDisplay(const ControllerState& c,bool uartAlive,uint16_t sequence) {
  using namespace PadView;
  display.fillScreen(BG);
  display.setTextSize(2);label(8,5,"DUALSENSE");display.setTextSize(1);
  label(160,7,c.connected?"CONTROLLER CONNECTED":uartAlive?"CONTROLLER OFFLINE":"WAITING FOR UART",
        c.connected?GREEN:AMBER);
  trigger(8,"L2",c.l2(),c.button(ControllerState::L2));
  trigger(181,"R2",c.r2(),c.button(ControllerState::R2));
  display.fillRoundRect(22,43,276,111,25,PANEL);
  display.drawRoundRect(22,43,276,111,25,EDGE);
  key(33,38,42,"L1",c.button(ControllerState::L1));
  key(245,38,42,"R1",c.button(ControllerState::R1));

  // Touchpad surface with independently colored contacts.
  display.fillRoundRect(108,51,104,43,5,BG);
  display.drawRoundRect(108,51,104,43,5,c.button(ControllerState::TOUCH_CLICK)?GREEN:EDGE);
  for(unsigned i=0;i<2;++i) if(c.touchActive(i)) {
    int x=113+uint32_t(min(c.touchX(i),uint16_t(1919)))*93/1919;
    int y=56+uint32_t(min(c.touchY(i),uint16_t(1079)))*32/1079;
    display.fillCircle(x,y,3,i?AMBER:BLUE);
  }
  key(80,58,20,"C",c.button(ControllerState::CREATE));
  key(220,58,20,"O",c.button(ControllerState::OPTIONS));
  key(43,66,20,"U",c.up());key(23,82,20,"L",c.left());
  key(63,82,20,"R",c.right());key(43,98,20,"D",c.down());
  face(267,65,'T',c.button(ControllerState::TRIANGLE),GREEN);
  face(244,86,'S',c.button(ControllerState::SQUARE),0xfbbf);
  face(290,86,'O',c.button(ControllerState::CIRCLE),RED);
  face(267,107,'X',c.button(ControllerState::CROSS),BLUE);
  stick(108,116,c.leftX(),c.leftY(),c.button(ControllerState::L3));
  stick(212,116,c.rightX(),c.rightY(),c.button(ControllerState::R3));
  key(143,103,34,"PS",c.button(ControllerState::PS));
  key(139,122,42,"MUTE",c.button(ControllerState::MUTE),AMBER);
  display.setTextColor(TEXT);
  display.setCursor(70,142);display.printf("L %4d %4d",c.leftX(),c.leftY());
  display.setCursor(190,142);display.printf("R %4d %4d",c.rightX(),c.rightY());

  display.setCursor(8,163);display.printf("GYRO  X:%6d Y:%6d Z:%6d",c.gyroX(),c.gyroY(),c.gyroZ());
  display.setCursor(8,175);display.printf("ACCEL X:%6d Y:%6d Z:%6d",c.accelX(),c.accelY(),c.accelZ());
  for(unsigned i=0;i<2;++i) {
    display.setTextColor(i?AMBER:BLUE);display.setCursor(8,187+12*i);
    if(c.touchActive(i)) display.printf("TOUCH%u ID:%3u X:%4u Y:%4u",i+1,c.touchId(i),c.touchX(i),c.touchY(i));
    else display.printf("TOUCH%u OFF",i+1);
  }
  display.setTextColor(MUTED);display.setCursor(8,213);
  if(c.connected) display.printf("BAT:%u/10 CHG:%u SEQ:%u",c.batteryLevelRaw(),c.chargeStateRaw(),sequence);
  else display.print("Controls neutral - awaiting controller");
  display.setCursor(8,227);
  display.printf("UART %s 230400  | C=Create O=Options",uartAlive?"OK":"--");
  display.swap();
  digitalWrite(LED_BUILTIN,c.connected?HIGH:((millis()/500)&1));
}
