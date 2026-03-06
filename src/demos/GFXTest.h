#pragma once
#include <Adafruit_GFX.h>

// Runs the GFX demo on two displays simultaneously.
// Every draw call is issued to both displays before any pause/flush,
// so TFT and iPhone stay frame-for-frame in sync.
// Pass the same display twice to run on a single display (original behaviour).
void runGFXTest(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t holdMs);
void runGFXOrientTest(Adafruit_GFX &a, Adafruit_GFX &b, uint32_t holdMs);