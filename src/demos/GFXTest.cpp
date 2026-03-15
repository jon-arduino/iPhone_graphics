#include "GFXTest.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Issue identical draw call to both displays.
#define BOTH(expr) \
    do             \
    {              \
        a.expr;    \
        b.expr;    \
    } while (0)

static void pauseAndFlush(Adafruit_GFX &a, Adafruit_GFX &b, uint32_t ms)
{
    a.flush();
    b.flush();
    if (ms)
        delay(ms);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GFX test scenes
// ─────────────────────────────────────────────────────────────────────────────

static void testFillScreen(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    (void)w;
    (void)h;
    BOTH(fillScreen(0x0000));
    pauseAndFlush(a, b, d);
    BOTH(fillScreen(0xF800));
    pauseAndFlush(a, b, d);
    BOTH(fillScreen(0x07E0));
    pauseAndFlush(a, b, d);
    BOTH(fillScreen(0x001F));
    pauseAndFlush(a, b, d);
    BOTH(fillScreen(0xFFFF));
    pauseAndFlush(a, b, d);
    BOTH(fillScreen(0x0000));
    pauseAndFlush(a, b, d);
}

static void testLines(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    for (int16_t x = 0; x < (int16_t)w; x += 6)
        BOTH(drawLine(0, 0, x, (int16_t)h - 1, 0x07FF));
    for (int16_t y = 0; y < (int16_t)h; y += 6)
        BOTH(drawLine(0, 0, (int16_t)w - 1, y, 0x07FF));
    pauseAndFlush(a, b, d);

    BOTH(fillScreen(0x0000));
    for (int16_t x = 0; x < (int16_t)w; x += 6)
        BOTH(drawLine((int16_t)w - 1, 0, x, (int16_t)h - 1, 0xF81F));
    for (int16_t y = 0; y < (int16_t)h; y += 6)
        BOTH(drawLine((int16_t)w - 1, 0, 0, y, 0xF81F));
    pauseAndFlush(a, b, d);

    BOTH(fillScreen(0x0000));
    for (int16_t x = 0; x < (int16_t)w; x += 6)
        BOTH(drawLine(0, (int16_t)h - 1, x, 0, 0xFFE0));
    for (int16_t y = 0; y < (int16_t)h; y += 6)
        BOTH(drawLine(0, (int16_t)h - 1, (int16_t)w - 1, y, 0xFFE0));
    pauseAndFlush(a, b, d);

    BOTH(fillScreen(0x0000));
    for (int16_t x = 0; x < (int16_t)w; x += 6)
        BOTH(drawLine((int16_t)w - 1, (int16_t)h - 1, x, 0, 0x07E0));
    for (int16_t y = 0; y < (int16_t)h; y += 6)
        BOTH(drawLine((int16_t)w - 1, (int16_t)h - 1, 0, y, 0x07E0));
    pauseAndFlush(a, b, d);
}

static void testFastLines(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    for (int16_t y = 0; y < (int16_t)h; y += 5)
        BOTH(drawFastHLine(0, y, (int16_t)w, 0xF800));
    for (int16_t x = 0; x < (int16_t)w; x += 5)
        BOTH(drawFastVLine(x, 0, (int16_t)h, 0x001F));
    pauseAndFlush(a, b, d);
}

static void testRects(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    int16_t cx = (int16_t)w / 2, cy = (int16_t)h / 2;
    int16_t color = 0xFFFF;
    for (int16_t i = 0; i < (int16_t)min(w, h); i += 6)
    {
        BOTH(drawRect(cx - i / 2, cy - i / 2, i, i, (uint16_t)color));
        color -= 0x0841;
    }
    pauseAndFlush(a, b, d);
}

static void testFilledRects(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    int16_t cx = (int16_t)w / 2, cy = (int16_t)h / 2;
    for (int16_t i = (int16_t)min(w, h); i > 0; i -= 10)
    {
        uint16_t c = (uint16_t)((i * 7) & 0xFFFF);
        BOTH(fillRect(cx - i / 2, cy - i / 2, i, i, c));
        BOTH(drawRect(cx - i / 2, cy - i / 2, i, i, 0xFFFF));
        pauseAndFlush(a, b, d / 4);
    }
    pauseAndFlush(a, b, d);
}

static void testCircles(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    int16_t cx = (int16_t)w / 2, cy = (int16_t)h / 2;
    int16_t rmax = (int16_t)min(w, h) / 2;
    for (int16_t r = 2; r < rmax; r += 6){
        BOTH(drawCircle(cx, cy, r, 0x07FF));
        }
    pauseAndFlush(a, b, d);

    BOTH(fillScreen(0x0000));
    for (int16_t r = rmax; r > 2; r -= 10)
    {
        BOTH(fillCircle(cx, cy, r, (uint16_t)((r * 31) & 0xFFFF)));
        pauseAndFlush(a, b, d / 5);
    }
    pauseAndFlush(a, b, d);
}

static void testTriangles(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    int16_t cx = (int16_t)w / 2, cy = (int16_t)h / 2;
    int16_t size = (int16_t)min(w, h) / 2;
    for (int16_t i = 0; i < size; i += 8)
        BOTH(drawTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
                          (uint16_t)(0xF800 - (i * 10))));
    pauseAndFlush(a, b, d);

    BOTH(fillScreen(0x0000));
    for (int16_t i = size; i > 0; i -= 12)
    {
        BOTH(fillTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
                          (uint16_t)((i * 17) & 0xFFFF)));
        pauseAndFlush(a, b, d / 5);
    }
    pauseAndFlush(a, b, d);
}

static void testRoundRects(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    int16_t cx = (int16_t)w / 2, cy = (int16_t)h / 2;
    for (int16_t i = 0; i < (int16_t)min(w, h); i += 8)
        BOTH(drawRoundRect(cx - i / 2, cy - i / 2, i, i, 10,
                           (uint16_t)(0x001F + i * 3)));
    pauseAndFlush(a, b, d);

    BOTH(fillScreen(0x0000));
    for (int16_t i = (int16_t)min(w, h); i > 0; i -= 12)
    {
        BOTH(fillRoundRect(cx - i / 2, cy - i / 2, i, i, 12,
                           (uint16_t)((i * 9) & 0xFFFF)));
        pauseAndFlush(a, b, d / 6);
    }
    pauseAndFlush(a, b, d);
}

static void testText(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h, uint32_t d)
{
    BOTH(fillScreen(0x0000));
    BOTH(setCursor(0, 0));
    BOTH(setTextSize(1));
    BOTH(setTextColor(0xFFFF));
    BOTH(println("Adafruit-style GFX Test"));
    pauseAndFlush(a, b, d);

    BOTH(setTextColor(0xFFE0));
    BOTH(println("ESP32 -> iPhone renderer"));
    BOTH(setTextColor(0x07FF));
    BOTH(print("Size 1: "));
    BOTH(println("Hello!"));
    pauseAndFlush(a, b, d);

    BOTH(setTextSize(2));
    BOTH(setTextColor(0xF81F));
    BOTH(setCursor(0, 30));
    BOTH(println("Size 2"));
    pauseAndFlush(a, b, d);

    BOTH(setTextSize(1));
    BOTH(setTextColor(0x07E0));
    BOTH(setCursor(0, (int16_t)h - 20));
    BOTH(print("W="));
    BOTH(print(w));
    BOTH(print(" H="));
    BOTH(println(h));
    pauseAndFlush(a, b, d);
    delay(2000);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Orient test helpers — restored from original GFXOrientTest.cpp
// ─────────────────────────────────────────────────────────────────────────────

static void cornerLabel(Adafruit_GFX &a, Adafruit_GFX &b,
                        int16_t x, int16_t y,
                        const char *txt,
                        uint16_t color,
                        uint16_t boxW, uint16_t boxH)
{
    BOTH(drawRect(x, y, boxW, boxH, color));
    BOTH(setTextSize(2));
    BOTH(setTextColor(color));
    BOTH(setCursor(x + 4, y + 6));
    BOTH(print(txt));
}

static void wrapTest(Adafruit_GFX &a, Adafruit_GFX &b,
                     int16_t x, int16_t y, int16_t w, int16_t h,
                     uint16_t color)
{
    BOTH(drawRect(x, y, w, h, color));
    BOTH(setCursor(x + 2, y + 2));
    BOTH(setTextSize(1));
    BOTH(setTextColor(color));
    BOTH(setTextWrap(true));
    BOTH(print("WRAP TEST: 1234567890 ABCDEFGHIJ KLMNOPQRST UVWXYZ"));
}

static void drawIndicators(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h)
{
    uint16_t dim = 0x7BEF;
    BOTH(drawRect(0, 0, w, h, dim));
    BOTH(drawFastHLine(0, h / 2, w, dim));
    BOTH(drawFastVLine(w / 2, 0, h, dim));

    int16_t cx = w / 2, cy = h / 2;
    BOTH(drawTriangle(cx, cy - 30, cx - 25, cy + 20, cx + 25, cy + 20, 0xFFE0));
    BOTH(drawLine(0, 0, (int16_t)w - 1, (int16_t)h - 1, 0xF81F));
}

static void orientScreen(Adafruit_GFX &a, Adafruit_GFX &b, uint16_t w, uint16_t h)
{
    BOTH(fillScreen(0x0000));
    drawIndicators(a, b, w, h);

    const int16_t boxW = 64, boxH = 32;
    cornerLabel(a, b, 2, 2, "UL", 0xF800, boxW, boxH);
    cornerLabel(a, b, (int16_t)w - boxW - 2, 2, "UR", 0x001F, boxW, boxH);
    cornerLabel(a, b, 2, (int16_t)h - boxH - 2, "LL", 0x07E0, boxW, boxH);
    cornerLabel(a, b, (int16_t)w - boxW - 2, (int16_t)h - boxH - 2, "LR", 0xFFFF, boxW, boxH);

    wrapTest(a, b, (int16_t)(w / 2 - 60), 6, 120, 34, 0x07FF);

    BOTH(setTextWrap(false));
    BOTH(setTextSize(1));
    BOTH(setTextColor(0xFFFF));
    BOTH(setCursor(6, (int16_t)(h / 2 + 10)));
    BOTH(println("NL1"));
    BOTH(println("NL2"));
    BOTH(println("NL3"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void runGFXTest(Adafruit_GFX &a, Adafruit_GFX &b,
                uint16_t width, uint16_t height, uint32_t holdMs)
{
    BOTH(setRotation(0));
    pauseAndFlush(a, b, holdMs);

    testFillScreen(a, b, width, height, holdMs);
    testLines(a, b, width, height, holdMs);
    testFastLines(a, b, width, height, holdMs);
    testRects(a, b, width, height, holdMs);
    testFilledRects(a, b, width, height, holdMs);
    testCircles(a, b, width, height, holdMs);
    testRoundRects(a, b, width, height, holdMs);
    testTriangles(a, b, width, height, holdMs);
    testText(a, b, width, height, holdMs);

    delay(5000);

    BOTH(fillScreen(0x0000));
    BOTH(setTextSize(2));
    BOTH(setTextColor(0xFFFF));
    BOTH(setCursor(10, 10));
    BOTH(println("GFX TEST DONE"));
    BOTH(setTextSize(1));
    BOTH(setCursor(10, 40));
    BOTH(println("If you saw each scene,"));
    BOTH(println("the transport + parser"));
    BOTH(println("are working."));
    pauseAndFlush(a, b, 0);
}

void runGFXOrientTest(Adafruit_GFX &a, Adafruit_GFX &b, uint32_t holdMs)
{
    for (uint8_t r = 0; r < 4; r++)
    {
        BOTH(setRotation(r));

        // Use display a's dimensions to drive layout (TFT is authoritative)
        uint16_t w = (uint16_t)a.width();
        uint16_t h = (uint16_t)a.height();

        orientScreen(a, b, w, h);

        BOTH(setTextSize(1));
        BOTH(setTextColor(0xFFFF));
        BOTH(setCursor((int16_t)(w / 2 - 20), (int16_t)(h - 12)));
        BOTH(print("rot="));
        BOTH(print(r));

        pauseAndFlush(a, b, holdMs);
    }

    BOTH(setRotation(0));
    pauseAndFlush(a, b, 50);
}