#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "telemetry/TelemetryPacket.h"

// Color fallbacks if ST77XX_* isn't included from ST7735 library.
// (RGB565)
#ifndef ST77XX_BLACK
#define ST77XX_BLACK 0x0000
#define ST77XX_WHITE 0xFFFF
#define ST77XX_CYAN 0x07FF
#define ST77XX_YELLOW 0xFFE0
#endif

class DisplayModule
{
public:
    // Render to any Adafruit_GFX target (ST7735, iPhone driver, etc.)
    explicit DisplayModule(Adafruit_GFX &display);

    // Draws the static UI (banner + labels). Does NOT initialize hardware.
    void begin(uint8_t rotation = 1);

    // Update only the numeric values.
    void renderTelemetry(const TelemetryPacket &pkt);

private:
    Adafruit_GFX &_d;

    void drawLabel(int16_t x, int16_t y, const char *label);
    void drawValue(int16_t x, int16_t y, float value, uint8_t decimals);
};
