#include <Adafruit_NeoPixel.h>
#include "config.h"

// ============ LED OBJECT ============
Adafruit_NeoPixel ledStrip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============ LED INITIALIZATION ============
void initLED() {
  ledStrip.begin();
  ledStrip.setBrightness(LED_BRIGHTNESS);
  ledStrip.clear();
  ledStrip.show();
  Serial.println("LED initialized");
}

// ============ LED COLOR CONTROL ============
void setLEDColor(uint8_t red, uint8_t green, uint8_t blue) {
  ledStrip.setPixelColor(0, ledStrip.Color(red, green, blue));
  ledStrip.show();
  // Serial.printf("LED set to: R=%d, G=%d, B=%d\n", red, green, blue);
}

// ============ LED STATUS INDICATORS ============
void ledStatusOff() {
  ledStrip.setBrightness(LED_BRIGHTNESS);
  setLEDColor(0, 0, 0);
}

void ledStatusRed() {
  ledStrip.setBrightness(LED_BRIGHTNESS);
  setLEDColor(255, 0, 0);
}

void ledStatusGreen() {
  ledStrip.setBrightness(LED_BRIGHTNESS);
  setLEDColor(0, 255, 0);
}

void ledStatusGrasping() {
  // Bright green for grasping detection
  ledStrip.setBrightness(LED_BRIGHTNESS_GRASPING);
  setLEDColor(0, 255, 0);
}

void ledStatusBlue() {
  ledStrip.setBrightness(LED_BRIGHTNESS);
  setLEDColor(0, 0, 255);
}

void ledStatusYellow() {
  ledStrip.setBrightness(LED_BRIGHTNESS);
  setLEDColor(255, 255, 0);
}

void ledStatusPurple() {
  ledStrip.setBrightness(LED_BRIGHTNESS);
  setLEDColor(128, 0, 128);
}
