// Display_iphone.h
#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "graphics/Graphics.h"
#include "telemetry/TelemetryPacket.h"

class Display_iPhone
{
public:
    explicit Display_iPhone(Graphics &gfx);

    // Initialize display once, draw static labels once
    void begin();

    // Update only numeric fields
    void renderTelemetry(const TelemetryPacket &pkt);

private:
    Graphics &_gfx;

    // Layout constants tuned to match your TFT code
    static constexpr int16_t kBaseW = 128;
    static constexpr int16_t kBaseH = 160;

    static constexpr int16_t kLabelX = 0;
    static constexpr int16_t kValueX = 50;

    static constexpr int16_t kRowStep = 12;
    static constexpr int16_t kGroupGap = 6;

    // Value field erase box (match your “80x10”)
    static constexpr int16_t kValueW = 80;
    static constexpr int16_t kValueH = 10;

    // RGB565 colors (approx ST77XX palette)
    static constexpr Color C_BLACK = 0x0000;
    static constexpr Color C_WHITE = 0xFFFF;
    static constexpr Color C_YELLOW = 0xFFE0;
    static constexpr Color C_CYAN = 0x07FF;

    void drawLabel(int16_t x, int16_t y, const char *label);
    void drawValue(int16_t x, int16_t y, float value, uint8_t decimals);
};
