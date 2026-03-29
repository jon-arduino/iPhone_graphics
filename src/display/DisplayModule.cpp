#include "DisplayModule.h"

DisplayModule::DisplayModule(Adafruit_GFX &display)
    : _d(display)
{
}

// -------------------------------
// Draw static labels ONCE
// Works on any Adafruit_GFX display
// -------------------------------
void DisplayModule::begin(uint8_t rotation)
{
    // For a real TFT this rotates addressing; for iPhone driver this maps logically too.
    _d.setRotation(rotation);

    _d.fillScreen(ST77XX_BLACK);

    // --- Startup Banner ---
    _d.setTextColor(ST77XX_YELLOW);
    _d.setTextSize(2);
    _d.setCursor(10, 10);
    _d.print("Telemetry");

    _d.setTextSize(1);
    _d.setCursor(10, 30);
    _d.print("System Ready");
    delay(500);

    // Clear banner screen (matches your old behavior)
    _d.fillScreen(ST77XX_BLACK);

    // -------------------------------
    // Draw static labels ONCE
    // These never change, so no flicker
    // -------------------------------
    _d.setTextColor(ST77XX_CYAN);
    _d.setTextSize(1);

    int y = 0;

    drawLabel(0, y, "Lat");
    y += 12;
    drawLabel(0, y, "Lng");
    y += 12;
    drawLabel(0, y, "Alt");
    y += 12;
    drawLabel(0, y, "Speed");
    y += 12;
    drawLabel(0, y, "Course");
    y += 12;

    y += 6;

    drawLabel(0, y, "V-N");
    y += 12;
    drawLabel(0, y, "V-E");
    y += 12;
    drawLabel(0, y, "Climb");
    y += 12;

    y += 6;

    drawLabel(0, y, "Sats");
    y += 12;
    drawLabel(0, y, "HDOP");
}

// -------------------------------
// Helper: draw static label text
// -------------------------------
void DisplayModule::drawLabel(int16_t x, int16_t y, const char *label)
{
    _d.setCursor(x, y);
    _d.print(label);
}

// -------------------------------
// Helper: erase old value + draw new one
// Only redraws the numeric field
// -------------------------------
void DisplayModule::drawValue(int16_t x, int16_t y, float value, uint8_t decimals)
{
    // Erase previous value area (80px wide, 10px tall)
    _d.fillRect(x, y, 80, 10, ST77XX_BLACK);
    // Draw updated value
    _d.setCursor(x, y);
    
    _d.print(value, decimals);
}

// -------------------------------
// Main telemetry renderer
// Flicker-free: only updates values
// -------------------------------
void DisplayModule::renderTelemetry(const TelemetryPacket &pkt)
{
    int valueX = 50; // aligned to the right of labels
    int y = 0;
    _d.setTextColor(ST77XX_WHITE);    // all values in white; (labels are cyan)

    drawValue(valueX, y, pkt.lat, 6);
    y += 12;
    drawValue(valueX, y, pkt.lng, 6);
    y += 12;
    drawValue(valueX, y, pkt.alt, 1);
    y += 12;
    drawValue(valueX, y, pkt.speed, 2);
    y += 12;
    drawValue(valueX, y, pkt.course, 1);
    y += 12;

    y += 6;

    drawValue(valueX, y, pkt.vNorth, 3);
    y += 12;
    drawValue(valueX, y, pkt.vEast, 3);
    y += 12;
    drawValue(valueX, y, pkt.climb, 3);
    y += 12;

    y += 6;

    drawValue(valueX, y, pkt.sats, 0);
    y += 12;
    drawValue(valueX, y, pkt.hdop, 2);
}
