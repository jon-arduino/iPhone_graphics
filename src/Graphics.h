// Graphics.h
#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "GraphicsProtocol.h"
#include "GraphicsTransport.h"

// Forward-declare Adafruit GFX font type (to avoid pulling in Adafruit_GFX.h here)
//struct GFXfont;
#include <Adafruit_GFX.h> // for GFXfont

// Match your protocol color type (RGB565)
using Color = uint16_t;

class Graphics : public Print
{
public:
    explicit Graphics(GraphicsTransport &transport);

    // System
    void begin(uint16_t width, uint16_t height);
    void clear(Color color = 0x0000);
    void flush();

    // Pixels & Lines
    void drawPixel(int16_t x, int16_t y, Color color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, Color color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, Color color);

    // Rectangles
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color);
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, Color color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, Color color);

    // Circles & Triangles
    void drawCircle(int16_t x, int16_t y, int16_t r, Color color);
    void fillCircle(int16_t x, int16_t y, int16_t r, Color color);
    void drawTriangle(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2,
                      Color color);
    void fillTriangle(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2,
                      Color color);

    // Bitmaps (1-bit monochrome like Adafruit_GFX::drawBitmap)
    void drawBitmap(int16_t x, int16_t y,
                    const uint8_t *bitmap,
                    int16_t w, int16_t h,
                    Color color);

    void drawBitmap(int16_t x, int16_t y,
                    const uint8_t *bitmap,
                    int16_t w, int16_t h,
                    Color fg, Color bg);

    // Text API (matches Adafruit_GFX style)
    void setCursor(int16_t x, int16_t y);
    void setTextColor(Color color);
    void setTextColor(Color fg, Color bg);
    void setTextSize(uint8_t size);
    void setTextWrap(bool wrap);
    void cp437(bool enable = true);
    void setFont(const GFXfont *font);

    // Print override
    size_t write(uint8_t c) override;

private:
    GraphicsTransport &_transport;

    // Local mirrored state (optional but handy)
    int16_t _cursorX = 0;
    int16_t _cursorY = 0;
    Color _textColor = 0xFFFF;
    Color _textBgColor = 0x0000;
    uint8_t _textSize = 1;
    bool _wrap = true;
    bool _cp437 = false;
    const GFXfont *_font = nullptr;

    // Framed command senders
    void sendCommand(uint8_t cmd, const void *payload, uint16_t len);
    void sendCommandWithTail(uint8_t cmd,
                             const void *fixed, uint16_t fixedLen,
                             const void *tail, uint16_t tailLen);
};
