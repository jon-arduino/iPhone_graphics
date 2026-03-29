#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

// Runs the Adafruit-style GFX demo against ANY Adafruit_GFX display
void runGFXTest(Adafruit_GFX &d, uint16_t w, uint16_t h, uint32_t holdMs);