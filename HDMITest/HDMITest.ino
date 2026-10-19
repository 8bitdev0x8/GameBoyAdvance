// Standalone Adafruit Feather RP2350 HSTX video test.
// Feather -> 22-pin FPC -> Adafruit HSTX-to-DVI adapter -> HDMI monitor.
// Requires Adafruit DVI HSTX and Adafruit GFX libraries.
// Library sets the RP2350 clock to 264 MHz.
#include <Adafruit_dvhstx.h>

#if !defined(ARDUINO_ADAFRUIT_FEATHER_RP2350_HSTX) && !defined(ADAFRUIT_FEATHER_RP2350_HSTX)
#error Select Adafruit Feather RP2350 HSTX in the Arduino Tools > Board menu.
#endif

DVHSTX16 display(ADAFRUIT_FEATHER_RP2350_CFG, DVHSTX_RESOLUTION_320x240);
constexpr uint16_t BLACK=0x0000, WHITE=0xffff;
const uint16_t bars[]={0xffff,0xffe0,0x07ff,0x07e0,0xf81f,0xf800,0x001f,0x0000};

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  if (!display.begin()) {
    // Rapid LED flashing means framebuffer/display initialization failed.
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH); delay(100);
      digitalWrite(LED_BUILTIN, LOW); delay(100);
    }
  }
  display.fillScreen(BLACK);
  display.setTextWrap(false);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(16,12); display.print("FEATHER RP2350");
  display.setTextSize(1);
  display.setCursor(16,36); display.print("HSTX VIDEO TEST - 640x480 / 60 Hz");
  for (int i=0;i<8;++i) display.fillRect(i*40,56,40,80,bars[i]);
  // Grayscale ramp checks tonal separation; outer border checks cropping.
  for (int x=0;x<288;++x) {
    uint8_t shade=uint32_t(x)*255/287;
    display.drawFastVLine(16+x,148,20,display.color565(shade,shade,shade));
  }
  display.drawRect(0,0,320,240,WHITE);
  display.setCursor(16,181); display.print("Moving square + counter = code running");
}

void loop() {
  static uint32_t lastFrame=0,frames=0;
  if (uint32_t(millis()-lastFrame)<33) return;
  lastFrame=millis();
  display.fillRect(16,196,288,28,BLACK);
  display.fillRect(16+(frames%273),198,16,12,0x07e0);
  display.setTextColor(WHITE,BLACK);
  display.setCursor(16,216);
  display.print("Uptime: "); display.print(millis()/1000);
  display.print("s  Frames: "); display.print(frames++);
  digitalWrite(LED_BUILTIN,(millis()/500)&1);
}
