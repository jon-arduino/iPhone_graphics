#include "GFXOrientTest.h"

// flush if supported (iPhone), no-op otherwise (ST7735)
template <typename T>
static auto flushIfSupported(T &d) -> decltype(d.flush(), void()) { d.flush(); }
static void flushIfSupported(...) {}

static inline void flushPause(Adafruit_GFX &d, uint32_t ms)
{
    flushIfSupported(d);
    if (ms)
        delay(ms);
}

static void cornerLabel(Adafruit_GFX &d,
                        int16_t x, int16_t y,
                        const char *txt,
                        uint16_t color,
                        uint16_t boxW, uint16_t boxH)
{
    d.drawRect(x, y, boxW, boxH, color);
    d.setTextSize(2);
    d.setTextColor(color);
    d.setCursor(x + 4, y + 6);
    d.print(txt);
}

static void wrapTest(Adafruit_GFX &d, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    d.drawRect(x, y, w, h, color);
    d.setCursor(x + 2, y + 2);
    d.setTextSize(1);
    d.setTextColor(color);
    d.setTextWrap(true);
    d.print("WRAP TEST: 1234567890 ABCDEFGHIJ KLMNOPQRST UVWXYZ");
}

static void drawIndicators(Adafruit_GFX &d, uint16_t w, uint16_t h)
{
    uint16_t dim = 0x7BEF;
    d.drawRect(0, 0, w, h, dim);
    d.drawFastHLine(0, h / 2, w, dim);
    d.drawFastVLine(w / 2, 0, h, dim);

    uint16_t triC = 0xFFE0;
    int16_t cx = w / 2;
    int16_t cy = h / 2;
    d.drawTriangle(
        cx, cy - 30,
        cx - 25, cy + 20,
        cx + 25, cy + 20,
        triC);

    d.drawLine(0, 0, (int16_t)w - 1, (int16_t)h - 1, 0xF81F);
}

static void orientScreen(Adafruit_GFX &d, uint16_t w, uint16_t h)
{
    d.fillScreen(0x0000);
    drawIndicators(d, w, h);

    const int16_t boxW = 64;
    const int16_t boxH = 32;

    cornerLabel(d, 2, 2, "UL", 0xF800, boxW, boxH);
    cornerLabel(d, (int16_t)w - boxW - 2, 2, "UR", 0x001F, boxW, boxH);
    cornerLabel(d, 2, (int16_t)h - boxH - 2, "LL", 0x07E0, boxW, boxH);
    cornerLabel(d, (int16_t)w - boxW - 2, (int16_t)h - boxH - 2, "LR", 0xFFFF, boxW, boxH);

    wrapTest(d, (int16_t)(w / 2 - 60), 6, 120, 34, 0x07FF);

    d.setTextWrap(false);
    d.setTextSize(1);
    d.setTextColor(0xFFFF);
    d.setCursor(6, (int16_t)(h / 2 + 10));
    d.println("NL1");
    d.println("NL2");
    d.println("NL3");
}

void runGFXOrientTest(Adafruit_GFX &d, uint32_t holdMs)
{

    // Show 0..3 as regression
    for (uint8_t r = 0; r < 4; r++)
    {
        d.setRotation(r);

        uint16_t dw = (uint16_t)d.width();
        uint16_t dh = (uint16_t)d.height();

        orientScreen(d, dw, dh);

        d.setTextSize(1);
        d.setTextColor(0xFFFF);
        d.setCursor((int16_t)(dw / 2 - 20), (int16_t)(dh - 12));
        d.print("rot=");
        d.print(r);

        flushPause(d, holdMs);
    }

    d.setRotation(0);
    flushPause(d, 50);
}