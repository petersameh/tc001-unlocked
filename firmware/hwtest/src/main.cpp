// TC001 hardware test: verify pin map + LED wiring before writing the real firmware.
#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>

// Assumed TC001 pin map (to verify)
#define PIN_MATRIX 32
#define PIN_BUZZER 15
#define PIN_BTN_LEFT 26
#define PIN_BTN_MID 27
#define PIN_BTN_RIGHT 14
#define PIN_LDR 35
#define PIN_BAT 34
#define PIN_SDA 21
#define PIN_SCL 22

#define NUM_LEDS 256
CRGB leds[NUM_LEDS];

const int btnPins[3] = {PIN_BTN_LEFT, PIN_BTN_MID, PIN_BTN_RIGHT};
const char *btnNames[3] = {"LEFT(26)", "MIDDLE(27)", "RIGHT(14)"};
int lastBtn[3];

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);  // silence buzzer first

  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== TC001 HWTEST ===");

  FastLED.addLeds<WS2812B, PIN_MATRIX, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  FastLED.clear();
  // Raw LED index markers -> reveals physical wiring order
  leds[0] = CRGB::Red;      // index 0
  leds[1] = CRGB::Green;    // index 1
  leds[2] = CRGB::Green;    // index 2
  leds[31] = CRGB::Blue;    // index 31 (end of first strip row?)
  leds[32] = CRGB::White;   // index 32 (start of second row?)
  leds[255] = CRGB::Yellow; // last LED
  FastLED.show();

  for (int i = 0; i < 3; i++) {
    pinMode(btnPins[i], INPUT_PULLUP);
    lastBtn[i] = digitalRead(btnPins[i]);
  }
  Serial.printf("Buttons idle levels: L=%d M=%d R=%d\n", lastBtn[0], lastBtn[1], lastBtn[2]);

  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.print("I2C scan:");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
  }
  Serial.printf("  (%d device(s))\n", found);
}

uint32_t lastAdc = 0;
void loop() {
  for (int i = 0; i < 3; i++) {
    int v = digitalRead(btnPins[i]);
    if (v != lastBtn[i]) {
      Serial.printf("BUTTON %s -> %s\n", btnNames[i], v ? "HIGH" : "LOW");
      lastBtn[i] = v;
    }
  }
  if (millis() - lastAdc > 2000) {
    lastAdc = millis();
    Serial.printf("ADC LDR(35)=%d  BAT(34)=%d\n", analogRead(PIN_LDR), analogRead(PIN_BAT));
  }
  delay(10);
}
