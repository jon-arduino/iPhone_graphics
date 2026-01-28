#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "TelemetryPacket.h"

class DisplayModule
{
public:
    DisplayModule();
    void begin();
    void renderTelemetry(const TelemetryPacket &pkt);

private:
    Adafruit_ST7735 tft;

    void drawLabel(int16_t x, int16_t y, const char *label);
    void drawValue(int16_t x, int16_t y, float value, uint8_t decimals);
};