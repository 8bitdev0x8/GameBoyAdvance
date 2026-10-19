// Adafruit Feather RP2350 with HSTX; Earle Philhower Arduino-Pico core.
#include <Arduino.h>
#include "UartProtocol.h"
#include "ControllerState.h"
#include "ControllerDisplay.h"
constexpr uint32_t LINK_TIMEOUT_MS=250;
PadWire::Parser parser;
ControllerState controller;
uint16_t packetSequence=0;
uint32_t lastValidPacketMs=0;
bool havePacket=false;
void setup() {
  Serial1.setRX(1); // Feather RX label
  Serial1.setTX(0);
  Serial1.setFIFOSize(1024); // Buffers incoming UART while video waits for retrace.
  Serial1.begin(230400);
  controller.clear();
  beginControllerDisplay();
}
void loop() {
  uint8_t packet[PadWire::PACKET_SIZE];
  // Bounded work, including under sustained serial noise.
  for(unsigned budget=0;budget<512 && Serial1.available();++budget) {
    if(parser.push(uint8_t(Serial1.read()),packet)) {
      lastValidPacketMs=millis(); havePacket=true;
      packetSequence=uint16_t(packet[2])|(uint16_t(packet[3])<<8);
      if(packet[1]==1) {
        memcpy(controller.raw,packet+4,78);
        controller.connected=true;
      } else controller.clear();
    }
  }
  if(controller.connected &&
     (!havePacket || uint32_t(millis()-lastValidPacketMs)>=LINK_TIMEOUT_MS))
    controller.clear();
  static uint32_t lastDrawMs=0;
  if(uint32_t(millis()-lastDrawMs)>=33) {
    lastDrawMs=millis();
    const bool linkAlive=havePacket && uint32_t(millis()-lastValidPacketMs)<LINK_TIMEOUT_MS;
    drawControllerDisplay(controller,linkAlive,packetSequence);
  }
  // Application logic goes here. Check controller.connected before acting.
  // int steering=controller.leftX();
  // uint8_t throttle=controller.r2();
  // bool crossHeld=controller.button(ControllerState::CROSS);
  // Drive outputs to a neutral state when disconnected; do not retain old outputs.
}
