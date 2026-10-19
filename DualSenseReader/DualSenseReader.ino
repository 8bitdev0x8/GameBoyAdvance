// ESP32-D0WD + HamzaYslmn/esp-ps5. GPIO25 -> Feather RX; GND -> GND.
#include <Arduino.h>
#include <ps5Controller.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "UartProtocol.h"
constexpr int UART_TX_PIN = 25;
constexpr uint32_t UART_BAUD = 230400;
constexpr uint32_t SEND_INTERVAL_MS = 10;
constexpr uint32_t INPUT_TIMEOUT_MS = 100;
struct Report { uint8_t bytes[78]; uint32_t receivedMs; };
QueueHandle_t reports;
// Bluetooth task: bounded copy; never write UART or wait here.
void onInput() {
  const uint8_t* p = ps5.latestPacket;
  if (!p || p[0] != 0x31) return;
  Report r;
  memcpy(r.bytes, p, sizeof(r.bytes));
  r.receivedMs = millis();
  xQueueOverwrite(reports, &r);
}
void setup() {
  Serial2.begin(UART_BAUD, SERIAL_8N1, -1, UART_TX_PIN);
  reports = xQueueCreate(1, sizeof(Report));
  if (!reports) while (true) delay(1000);
  ps5.attach(onInput);
  ps5.begin(uint8_t(30));
}
void loop() {
  static Report latest = {};
  static bool haveReport = false, previouslyOnline = false;
  static uint32_t lastSend = 0;
  static uint16_t sequence = 0;
  const bool connected = ps5.isConnected(); // Services reconnection.
  Report incoming;
  if (xQueueReceive(reports, &incoming, 0) == pdTRUE) {
    latest = incoming;
    haveReport = true;
  }
  const uint32_t now = millis();
  const bool online = connected && haveReport &&
                      uint32_t(now-latest.receivedMs) < INPUT_TIMEOUT_MS;
  const uint32_t interval = online ? SEND_INTERVAL_MS : 100;
  if (online != previouslyOnline || uint32_t(now-lastSend) >= interval) {
    uint8_t frame[PadWire::FRAME_SIZE];
    const size_t length = PadWire::encode(latest.bytes, online, sequence, frame);
    if (Serial2.availableForWrite() >= int(length)) {
      Serial2.write(frame, length);
      ++sequence;
      lastSend = now;
      previouslyOnline = online;
    }
  }
  delay(1);
}
