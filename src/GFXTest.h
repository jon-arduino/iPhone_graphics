#pragma once
#include <Arduino.h>
#include "Graphics.h"

// Call this from your firmware after BLE is connected (or whenever you want).
// width/height should match the virtual display size you want on iPhone.
void runGFXTest(Graphics &gfx, uint16_t width, uint16_t height, uint32_t sceneDelayMs = 500);
