#include "Graphics.h"

// If you have gfx.setRotation(r) implemented, enable this.
#ifndef HAS_GFX_SET_ROTATION
#define HAS_GFX_SET_ROTATION 1
#endif

static inline void flushPause(Graphics &gfx, uint32_t ms)
{
    gfx.flush();
    if (ms)
        delay(ms);
}

// Draw a labeled corner box with matching outline color.
static void cornerLabel(Graphics &gfx,
                        int16_t x, int16_t y,
                        const char *txt,
                        uint16_t color,
                        uint16_t boxW, uint16_t boxH)
{
    gfx.drawRect(x, y, boxW, boxH, color);

    gfx.setTextSize(2);
    gfx.setTextColor(color);
    gfx.setCursor(x + 4, y + 6);
    gfx.print(txt);
}

static void wrapTest(Graphics &gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    gfx.drawRect(x, y, w, h, color);
    gfx.setCursor(x + 2, y + 2);
    gfx.setTextSize(1);
    gfx.setTextColor(color);
    gfx.setTextWrap(true);

    gfx.print("WRAP TEST: 1234567890 ABCDEFGHIJ KLMNOPQRST UVWXYZ");
}

static void drawIndicators(Graphics &gfx, uint16_t w, uint16_t h)
{
    uint16_t dim = 0x7BEF; // ~50% gray
    gfx.drawRect(0, 0, w, h, dim);
    gfx.drawFastHLine(0, h / 2, w, dim);
    gfx.drawFastVLine(w / 2, 0, h, dim);

    // Up-pointing triangle
    uint16_t triC = 0xFFE0; // yellow
    int16_t cx = (int16_t)w / 2;
    int16_t cy = (int16_t)h / 2;
    gfx.drawTriangle(
        cx, cy - 30,      // apex
        cx - 25, cy + 20, // base left
        cx + 25, cy + 20, // base right
        triC);

    // Diagonal UL->LR
    gfx.drawLine(0, 0, (int16_t)w - 1, (int16_t)h - 1, 0xF81F);
}

static void orientScreen(Graphics &gfx, uint16_t w, uint16_t h, uint8_t rot)
{
    gfx.clear(0x0000);

    drawIndicators(gfx, w, h);

    const int16_t boxW = 64;
    const int16_t boxH = 32;

    cornerLabel(gfx, 2, 2, "UL", 0xF800, boxW, boxH);
    cornerLabel(gfx, (int16_t)w - boxW - 2, 2, "UR", 0x001F, boxW, boxH);
    cornerLabel(gfx, 2, (int16_t)h - boxH - 2, "LL", 0x07E0, boxW, boxH);
    cornerLabel(gfx, (int16_t)w - boxW - 2, (int16_t)h - boxH - 2, "LR", 0xFFFF, boxW, boxH);

    // Wrap test box (cyan)
    wrapTest(gfx, (int16_t)(w / 2 - 60), 6, 120, 34, 0x07FF);

    // Newline test: should go DOWN
    gfx.setTextWrap(false);
    gfx.setTextSize(1);
    gfx.setTextColor(0xFFFF);
    gfx.setCursor(6, (int16_t)(h / 2 + 10));
    gfx.println("NL1");
    gfx.println("NL2");
    gfx.println("NL3");

    // Rotation label (bottom-ish)
    gfx.setTextWrap(false);
    gfx.setTextSize(1);
    gfx.setTextColor(0xFFFF);
    gfx.setCursor((int16_t)(w / 2 - 20), (int16_t)(h - 12));
    gfx.print("rot=");
    gfx.print(rot);
}

void runGFXOrientTest(Graphics &gfx, uint16_t width, uint16_t height, uint32_t holdMs)
{
    // Physical surface size (rotation 0) on the iPhone
    gfx.begin(width, height);
    flushPause(gfx, 200);

#if HAS_GFX_SET_ROTATION
    for (uint8_t r = 0; r < 4; r++)
    {
        gfx.setRotation(r);

        // IMPORTANT:
        // For Adafruit_GFX semantics, logical W/H swap for rotations 1 and 3.
        uint16_t w = (r & 1) ? height : width;
        uint16_t h = (r & 1) ? width : height;

        orientScreen(gfx, w, h, r);
        flushPause(gfx, holdMs);
    }
#else
    orientScreen(gfx, width, height, 0);
    flushPause(gfx, holdMs);
#endif
}
