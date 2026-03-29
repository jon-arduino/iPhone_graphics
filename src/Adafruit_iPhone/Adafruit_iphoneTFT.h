#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

#include "graphics/Graphics.h" // <-- adjust path if needed

// A display-driver-like wrapper that exposes an Adafruit_GFX-compatible API
// but sends optimized drawing opcodes over your Graphics protocol.
class Adafruit_iPhoneTFT : public Adafruit_GFX
{
public:
    // "w,h" are the raw physical surface size you want the phone to allocate (rotation=0 basis)
    Adafruit_iPhoneTFT(Graphics &gfx, int16_t w, int16_t h);

    // Initializes remote surface (BEGIN) and optionally clears.
    void begin(uint16_t clearColor = 0x0000);

    // Optional convenience passthrough
    void flush();

    // ---- Adafruit_GFX required override ----
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;

    // ---- Transaction / core draw API overrides (for speed) ----
    void startWrite(void) override;
    void endWrite(void) override;

    void writePixel(int16_t x, int16_t y, uint16_t color) override;
    void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
    void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;

    // ---- Control API overrides ----
    void setRotation(uint8_t r) override;
    void invertDisplay(bool i) override;

    // ---- Basic draw API overrides (also used directly by sketches) ----
    void fillScreen(uint16_t color) override;
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

    // ---- Print override (this is the key for fast text) ----
    size_t write(uint8_t c) override;

    // These are NOT virtual in Adafruit_GFX, but we "hide" them so most sketches
    // calling on the concrete type will also immediately sync state to phone.
    // If code holds Adafruit_GFX&, the base versions run; we still sync lazily in write().
    void setCursor(int16_t x, int16_t y);
    void setTextColor(uint16_t c);
    void setTextColor(uint16_t c, uint16_t bg);
    void setTextSize(uint8_t s);
    void setTextSize(uint8_t sx, uint8_t sy);
    void setTextWrap(bool w);
    void cp437(bool x = true);

private:
    Graphics &_gfx;
    int16_t _rawW;
    int16_t _rawH;

    // Remote text state cache (what the phone currently thinks)
    bool _remoteValid = false;
    int16_t _rCursorX = 0;
    int16_t _rCursorY = 0;
    uint16_t _rTextColor = 0xFFFF;
    uint16_t _rTextBg = 0xFFFF; // Adafruit "transparent" uses same fg/bg
    uint8_t _rTextSizeX = 1;
    uint8_t _rTextSizeY = 1;
    bool _rWrap = true;
    bool _rCP437 = false;

    // Dirty flags (set when local Adafruit_GFX members may differ from remote)
    bool _dirtyCursor = true;
    bool _dirtyColor = true;
    bool _dirtySize = true;
    bool _dirtyWrap = true;
    bool _dirtyCP437 = true;

private:
    void markAllTextDirty();
    void syncTextStateIfNeeded();
};
