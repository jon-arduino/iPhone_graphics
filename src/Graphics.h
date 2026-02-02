#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Print.h>
#include "GraphicsTransport.h"
#include "GraphicsProtocol.h"

class GraphicsTransport; // Forward declaration

// -----------------------------------------------------------------------------
// Graphics Command Set Version
// -----------------------------------------------------------------------------
#define GFX_PROTOCOL_VERSION 1

// -----------------------------------------------------------------------------
// Color Format
// 16-bit RGB565 for compact transmission
// -----------------------------------------------------------------------------
typedef uint16_t Color;

// Helper macro for RGB565
constexpr Color RGB(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) |
           ((g & 0xFC) << 3) |
           (b >> 3);
}


// Forward declaration for Adafruit GFX font struct
typedef struct GFXfont GFXfont;

class Graphics : public Print
{
public:
    explicit Graphics(GraphicsTransport &transport);

    // Core initialization
    void begin(uint16_t width, uint16_t height);
    void clear(Color color);

    // Basic drawing
    void drawPixel(int16_t x, int16_t y, Color color);
    void drawLine(int16_t x0, int16_t y0,
                  int16_t x1, int16_t y1,
                  Color color);

    void drawFastHLine(int16_t x, int16_t y,
                       int16_t w, Color color);

    void drawFastVLine(int16_t x, int16_t y,
                       int16_t h, Color color);

    void drawRect(int16_t x, int16_t y,
                  int16_t w, int16_t h,
                  Color color);

    void fillRect(int16_t x, int16_t y,
                  int16_t w, int16_t h,
                  Color color);

    void drawRoundRect(int16_t x, int16_t y,
                       int16_t w, int16_t h,
                       int16_t r, Color color);

    void fillRoundRect(int16_t x, int16_t y,
                       int16_t w, int16_t h,
                       int16_t r, Color color);

    void drawCircle(int16_t x, int16_t y,
                    int16_t r, Color color);

    void fillCircle(int16_t x, int16_t y,
                    int16_t r, Color color);

    void drawTriangle(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2,
                      Color color);

    void fillTriangle(int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2,
                      Color color);

    // Bitmap API
    void drawBitmap(int16_t x, int16_t y,
                    const uint8_t *bitmap,
                    int16_t w, int16_t h,
                    Color color);

    void drawBitmap(int16_t x, int16_t y,
                    const uint8_t *bitmap,
                    int16_t w, int16_t h,
                    Color fg, Color bg);

    // Text API
    void setCursor(int16_t x, int16_t y);
    void setTextColor(Color color);
    void setTextColor(Color fg, Color bg);
    void setTextSize(uint8_t size);
    void setTextWrap(bool wrap);
    void cp437(bool enable);
    void setFont(const GFXfont *font);

    // Print class override
    size_t write(uint8_t c) override;

    // Flush pending operations
    void flush();

private:
    GraphicsTransport &_transport;

    // Internal state mirrors Adafruit_GFX
    int16_t _cursorX = 0;
    int16_t _cursorY = 0;
    Color _textColor = 0xFFFF;
    Color _textBgColor = 0x0000;
    uint8_t _textSize = 1;
    bool _wrap = true;
    bool _cp437 = false;
    const GFXfont *_font = nullptr;

    // Internal helper
    void sendCommand(uint8_t cmd, const void *payload, uint16_t len);
};