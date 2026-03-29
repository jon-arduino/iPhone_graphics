// Display_iphone.cpp
#include "Display_iphone.h"

Display_iPhone::Display_iPhone(Graphics &gfx)
    : _gfx(gfx)
{
}

void Display_iPhone::begin()
{
    // Create the iPhone “virtual TFT” surface.
    // This should match your physical display’s native pixels.
    _gfx.begin(kBaseW, kBaseH);

    // Match your ST7735 setup (you used setRotation(1))
    // This is the Adafruit_GFX rotation semantic.
    _gfx.setRotation(1);

    // Clear screen
    _gfx.clear(C_BLACK);

    // --- Startup Banner (same as your TFT code) ---
    _gfx.setTextColor(C_YELLOW);
    _gfx.setTextSize(2);
    _gfx.setCursor(10, 10);
    _gfx.print("Telemetry");

    _gfx.setTextSize(1);
    _gfx.setCursor(10, 30);
    _gfx.print("System Ready...");

    // If you want the banner visible briefly, uncomment:
     _gfx.flush();
     delay(500);

    _gfx.clear(C_BLACK);

    // -------------------------------
    // Draw static labels ONCE
    // -------------------------------
    _gfx.setTextColor(C_CYAN);
    _gfx.setTextSize(1);

    int16_t y = 0;

    drawLabel(kLabelX, y, "Lat");
    y += kRowStep;
    drawLabel(kLabelX, y, "Lng");
    y += kRowStep;
    drawLabel(kLabelX, y, "Alt");
    y += kRowStep;
    drawLabel(kLabelX, y, "Speed");
    y += kRowStep;
    drawLabel(kLabelX, y, "Course");
    y += kRowStep;

    y += kGroupGap;

    drawLabel(kLabelX, y, "V-N");
    y += kRowStep;
    drawLabel(kLabelX, y, "V-E");
    y += kRowStep;
    drawLabel(kLabelX, y, "Climb");
    y += kRowStep;

    y += kGroupGap;

    drawLabel(kLabelX, y, "Sats");
    y += kRowStep;
    drawLabel(kLabelX, y, "HDOP");

    // Push everything out
    _gfx.flush();
}

void Display_iPhone::drawLabel(int16_t x, int16_t y, const char *label)
{
    _gfx.setCursor(x, y);
    _gfx.print(label);
}

void Display_iPhone::drawValue(int16_t x, int16_t y, float value, uint8_t decimals)
{
    // Erase previous value area
    _gfx.fillRect(x, y, kValueW, kValueH, C_BLACK);

    // Draw updated value
    _gfx.setCursor(x, y);
    _gfx.setTextColor(C_WHITE);
    _gfx.print(value, decimals);
}

void Display_iPhone::renderTelemetry(const TelemetryPacket &pkt)
{
    int16_t y = 0;

    drawValue(kValueX, y, pkt.lat, 6);
    y += kRowStep;
    drawValue(kValueX, y, pkt.lng, 6);
    y += kRowStep;
    drawValue(kValueX, y, pkt.alt, 1);
    y += kRowStep;
    drawValue(kValueX, y, pkt.speed, 2);
    y += kRowStep;
    drawValue(kValueX, y, pkt.course, 1);
    y += kRowStep;

    y += kGroupGap;

    drawValue(kValueX, y, pkt.vNorth, 3);
    y += kRowStep;
    drawValue(kValueX, y, pkt.vEast, 3);
    y += kRowStep;
    drawValue(kValueX, y, pkt.climb, 3);
    y += kRowStep;

    y += kGroupGap;

    drawValue(kValueX, y, pkt.sats, 0);
    y += kRowStep;
    drawValue(kValueX, y, pkt.hdop, 2);

    _gfx.flush();
}
