#include "GFXTest.h"

// --------- helpers ----------
static inline void pauseAndFlush(Graphics &gfx, uint32_t ms)
{
    gfx.flush();
    if (ms)
        delay(ms);
}

static void testFillScreen(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    (void)w;
    (void)h;
    gfx.clear(0x0000);
    pauseAndFlush(gfx, d);
    gfx.clear(0xF800);
    pauseAndFlush(gfx, d); // Red
    gfx.clear(0x07E0);
    pauseAndFlush(gfx, d); // Green
    gfx.clear(0x001F);
    pauseAndFlush(gfx, d); // Blue
    gfx.clear(0xFFFF);
    pauseAndFlush(gfx, d); // White
    gfx.clear(0x0000);
    pauseAndFlush(gfx, d);
}

static void testLines(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);

    // Corner fan
    for (int16_t x = 0; x < (int16_t)w; x += 6)
    {
        gfx.drawLine(0, 0, x, (int16_t)h - 1, 0x07FF); // Cyan-ish
    }
    for (int16_t y = 0; y < (int16_t)h; y += 6)
    {
        gfx.drawLine(0, 0, (int16_t)w - 1, y, 0x07FF);
    }
    pauseAndFlush(gfx, d);

    gfx.clear(0x0000);
    for (int16_t x = 0; x < (int16_t)w; x += 6)
    {
        gfx.drawLine((int16_t)w - 1, 0, x, (int16_t)h - 1, 0xF81F); // Magenta
    }
    for (int16_t y = 0; y < (int16_t)h; y += 6)
    {
        gfx.drawLine((int16_t)w - 1, 0, 0, y, 0xF81F);
    }
    pauseAndFlush(gfx, d);

    gfx.clear(0x0000);
    for (int16_t x = 0; x < (int16_t)w; x += 6)
    {
        gfx.drawLine(0, (int16_t)h - 1, x, 0, 0xFFE0); // Yellow
    }
    for (int16_t y = 0; y < (int16_t)h; y += 6)
    {
        gfx.drawLine(0, (int16_t)h - 1, (int16_t)w - 1, y, 0xFFE0);
    }
    pauseAndFlush(gfx, d);

    gfx.clear(0x0000);
    for (int16_t x = 0; x < (int16_t)w; x += 6)
    {
        gfx.drawLine((int16_t)w - 1, (int16_t)h - 1, x, 0, 0x07E0); // Green
    }
    for (int16_t y = 0; y < (int16_t)h; y += 6)
    {
        gfx.drawLine((int16_t)w - 1, (int16_t)h - 1, 0, y, 0x07E0);
    }
    pauseAndFlush(gfx, d);
}

static void testFastLines(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);
    for (int16_t y = 0; y < (int16_t)h; y += 5)
    {
        gfx.drawFastHLine(0, y, (int16_t)w, 0xF800); // red
    }
    for (int16_t x = 0; x < (int16_t)w; x += 5)
    {
        gfx.drawFastVLine(x, 0, (int16_t)h, 0x001F); // blue
    }
    pauseAndFlush(gfx, d);
}

static void testRects(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);
    int16_t cx = (int16_t)w / 2;
    int16_t cy = (int16_t)h / 2;
    int16_t color = 0xFFFF;

    for (int16_t i = 0; i < (int16_t)min(w, h); i += 6)
    {
        int16_t x = cx - i / 2;
        int16_t y = cy - i / 2;
        gfx.drawRect(x, y, i, i, (uint16_t)color);
        color -= 0x0841; // step in RGB565 space-ish
    }
    pauseAndFlush(gfx, d);
}

static void testFilledRects(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);
    int16_t cx = (int16_t)w / 2;
    int16_t cy = (int16_t)h / 2;

    for (int16_t i = (int16_t)min(w, h); i > 0; i -= 10)
    {
        uint16_t c = (uint16_t)((i * 7) & 0xFFFF);
        gfx.fillRect(cx - i / 2, cy - i / 2, i, i, c);
        gfx.drawRect(cx - i / 2, cy - i / 2, i, i, 0xFFFF);
        pauseAndFlush(gfx, d / 4);
    }
    pauseAndFlush(gfx, d);
}

static void testCircles(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);
    int16_t cx = (int16_t)w / 2;
    int16_t cy = (int16_t)h / 2;
    int16_t rmax = (int16_t)min(w, h) / 2;

    for (int16_t r = 2; r < rmax; r += 6)
    {
        gfx.drawCircle(cx, cy, r, 0x07FF);
    }
    pauseAndFlush(gfx, d);

    gfx.clear(0x0000);
    for (int16_t r = rmax; r > 2; r -= 10)
    {
        uint16_t c = (uint16_t)((r * 31) & 0xFFFF);
        gfx.fillCircle(cx, cy, r, c);
        pauseAndFlush(gfx, d / 5);
    }
    pauseAndFlush(gfx, d);
}

static void testTriangles(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);
    int16_t cx = (int16_t)w / 2;
    int16_t cy = (int16_t)h / 2;
    int16_t size = (int16_t)min(w, h) / 2;

    for (int16_t i = 0; i < size; i += 8)
    {
        gfx.drawTriangle(
            cx, cy - i,
            cx - i, cy + i,
            cx + i, cy + i,
            (uint16_t)(0xF800 - (i * 10)));
    }
    pauseAndFlush(gfx, d);

    gfx.clear(0x0000);
    for (int16_t i = size; i > 0; i -= 12)
    {
        uint16_t c = (uint16_t)((i * 17) & 0xFFFF);
        gfx.fillTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i, c);
        pauseAndFlush(gfx, d / 5);
    }
    pauseAndFlush(gfx, d);
}

static void testRoundRects(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);
    int16_t cx = (int16_t)w / 2;
    int16_t cy = (int16_t)h / 2;

    for (int16_t i = 0; i < (int16_t)min(w, h); i += 8)
    {
        int16_t x = cx - i / 2;
        int16_t y = cy - i / 2;
        gfx.drawRoundRect(x, y, i, i, 10, (uint16_t)(0x001F + i * 3));
    }
    pauseAndFlush(gfx, d);

    gfx.clear(0x0000);
    for (int16_t i = (int16_t)min(w, h); i > 0; i -= 12)
    {
        uint16_t c = (uint16_t)((i * 9) & 0xFFFF);
        gfx.fillRoundRect(cx - i / 2, cy - i / 2, i, i, 12, c);
        pauseAndFlush(gfx, d / 6);
    }
    pauseAndFlush(gfx, d);
}

static void testText(Graphics &gfx, uint16_t w, uint16_t h, uint32_t d)
{
    gfx.clear(0x0000);

    gfx.setCursor(0, 0);
    gfx.setTextSize(1);
    gfx.setTextColor(0xFFFF);
    gfx.println("Adafruit-style GFX Test");
    pauseAndFlush(gfx, d);

    gfx.setTextColor(0xFFE0);
    gfx.println("ESP32 -> iPhone renderer");

    gfx.setTextColor(0x07FF);
    gfx.print("Size 1: ");
    gfx.println("Hello!");
    pauseAndFlush(gfx, d);

    gfx.setTextSize(2);
    gfx.setTextColor(0xF81F);
    gfx.setCursor(0, 30);
    gfx.println("Size 2");
    pauseAndFlush(gfx, d);

    gfx.setTextSize(1);
    gfx.setTextColor(0x07E0);
    gfx.setCursor(0, (int16_t)h - 20);
    gfx.print("W=");
    gfx.print(w);
    gfx.print(" H=");
    gfx.println(h);

    pauseAndFlush(gfx, d);
    delay(2000);
}

void runGFXTest(Graphics &gfx, uint16_t width, uint16_t height, uint32_t sceneDelayMs)
{
    // Tell iPhone the target display size
    gfx.begin(width, height);
    gfx.setRotation(0); // Landscape    
    pauseAndFlush(gfx, sceneDelayMs);

    // Similar ordering to Adafruit's graphicstest
    testFillScreen(gfx, width, height, sceneDelayMs);
    testLines(gfx, width, height, sceneDelayMs);
    testFastLines(gfx, width, height, sceneDelayMs);
    testRects(gfx, width, height, sceneDelayMs);
    testFilledRects(gfx, width, height, sceneDelayMs);
    testCircles(gfx, width, height, sceneDelayMs);
    testRoundRects(gfx, width, height, sceneDelayMs);
    testTriangles(gfx, width, height, sceneDelayMs);
    testText(gfx, width, height, sceneDelayMs);
    delay(5000); // Pause at end so you can admire the final screen before it goes away (or run next test)  

    // Final screen
    gfx.clear(0x0000);
    gfx.setTextSize(2);
    gfx.setTextColor(0xFFFF);
    gfx.setCursor(10, 10);
    gfx.println("GFX TEST DONE");
    gfx.setTextSize(1);
    gfx.setCursor(10, 40);
    gfx.println("If you saw each scene,");
    gfx.println("the transport + parser");
    gfx.println("are working.");
    pauseAndFlush(gfx, 0);
}
