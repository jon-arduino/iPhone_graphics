#include "DisplayModule.h"
#include <SPI.h>

#define TFT_CS 5
#define TFT_DC 2
#define TFT_RST 4

// ESP32-safe constructor: pass SPI object explicitly
DisplayModule::DisplayModule()
    : tft(&SPI, TFT_CS, TFT_DC, TFT_RST)
{
}

// -------------------------------
// Initialize display once
// Draw static labels once
// Leave screen ready for fast updates
// -------------------------------
void DisplayModule::begin()
{
    // Initialize SPI with explicit pin mapping
    SPI.begin(18, -1, 23, TFT_CS);

    // Initialize ST7735 controller
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    // --- Startup Banner ---
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("Telemetry");

    tft.setTextSize(1);
    tft.setCursor(10, 30);
    tft.print("System Ready");

    delay(800);
    tft.fillScreen(ST77XX_BLACK);

    // -------------------------------
    // Draw static labels ONCE
    // These never change, so no flicker
    // -------------------------------
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(1);

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
    tft.setCursor(x, y);
    tft.print(label);
}

// -------------------------------
// Helper: erase old value + draw new one
// Only redraws the numeric field
// -------------------------------
void DisplayModule::drawValue(int16_t x, int16_t y, float value, uint8_t decimals)
{
    // Erase previous value area (80px wide, 10px tall)
    tft.fillRect(x, y, 80, 10, ST77XX_BLACK);

    // Draw updated value
    tft.setCursor(x, y);
    tft.setTextColor(ST77XX_WHITE);
    tft.print(value, decimals);
}

// -------------------------------
// Main telemetry renderer
// Flicker-free: only updates values
// -------------------------------
void DisplayModule::renderTelemetry(const TelemetryPacket &pkt)
{
    int valueX = 50; // aligned to the right of labels
    int y = 0;

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