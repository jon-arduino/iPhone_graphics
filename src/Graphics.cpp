// Graphics.cpp
#include "Graphics.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
Graphics::Graphics(GraphicsTransport &transport)
    : _transport(transport) {}

// -----------------------------------------------------------------------------
// Internal helper: send a framed command packet (header + payload)
// -----------------------------------------------------------------------------
void Graphics::sendCommand(uint8_t cmd, const void *payload, uint16_t len)
{
    GfxPacketHeader hdr;
    hdr.cmd = cmd;
    hdr.length = len;

    _transport.send(reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr));

    if (payload && len > 0)
    {
        _transport.send(reinterpret_cast<const uint8_t *>(payload), len);
    }
}

// -----------------------------------------------------------------------------
// Internal helper: header + fixed payload + variable "tail" bytes (bitmaps/text)
// -----------------------------------------------------------------------------
void Graphics::sendCommandWithTail(uint8_t cmd,
                                   const void *fixed, uint16_t fixedLen,
                                   const void *tail, uint16_t tailLen)
{
    GfxPacketHeader hdr;
    hdr.cmd = cmd;
    hdr.length = fixedLen + tailLen;

    _transport.send(reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr));

    if (fixed && fixedLen)
    {
        _transport.send(reinterpret_cast<const uint8_t *>(fixed), fixedLen);
    }
    if (tail && tailLen)
    {
        _transport.send(reinterpret_cast<const uint8_t *>(tail), tailLen);
    }
}

// -----------------------------------------------------------------------------
// begin()
// -----------------------------------------------------------------------------
void Graphics::begin(uint16_t width, uint16_t height)
{
    GfxBeginPayload p{width, height};
    sendCommand(GFX_CMD_BEGIN, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// clear()
// -----------------------------------------------------------------------------
void Graphics::clear(Color color)
{
    GfxClearPayload p{color};
    sendCommand(GFX_CMD_CLEAR, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawPixel()
// -----------------------------------------------------------------------------
void Graphics::drawPixel(int16_t x, int16_t y, Color color)
{
    GfxDrawPixelPayload p{x, y, color};
    sendCommand(GFX_CMD_DRAW_PIXEL, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawLine()
// -----------------------------------------------------------------------------
void Graphics::drawLine(int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1,
                        Color color)
{
    GfxDrawLinePayload p{x0, y0, x1, y1, color};
    sendCommand(GFX_CMD_DRAW_LINE, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawFastHLine()
// -----------------------------------------------------------------------------
void Graphics::drawFastHLine(int16_t x, int16_t y,
                             int16_t w, Color color)
{
    GfxFastLinePayload p{x, y, w, color};
    sendCommand(GFX_CMD_DRAW_FAST_HLINE, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawFastVLine()
// -----------------------------------------------------------------------------
void Graphics::drawFastVLine(int16_t x, int16_t y,
                             int16_t h, Color color)
{
    GfxFastLinePayload p{x, y, h, color};
    sendCommand(GFX_CMD_DRAW_FAST_VLINE, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawRect()
// -----------------------------------------------------------------------------
void Graphics::drawRect(int16_t x, int16_t y,
                        int16_t w, int16_t h,
                        Color color)
{
    GfxRectPayload p{x, y, w, h, color};
    sendCommand(GFX_CMD_DRAW_RECT, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// fillRect()
// -----------------------------------------------------------------------------
void Graphics::fillRect(int16_t x, int16_t y,
                        int16_t w, int16_t h,
                        Color color)
{
    GfxRectPayload p{x, y, w, h, color};
    sendCommand(GFX_CMD_FILL_RECT, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawRoundRect()
// -----------------------------------------------------------------------------
void Graphics::drawRoundRect(int16_t x, int16_t y,
                             int16_t w, int16_t h,
                             int16_t r, Color color)
{
    GfxRoundRectPayload p{x, y, w, h, r, color};
    sendCommand(GFX_CMD_DRAW_ROUNDRECT, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// fillRoundRect()
// -----------------------------------------------------------------------------
void Graphics::fillRoundRect(int16_t x, int16_t y,
                             int16_t w, int16_t h,
                             int16_t r, Color color)
{
    GfxRoundRectPayload p{x, y, w, h, r, color};
    sendCommand(GFX_CMD_FILL_ROUNDRECT, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawCircle()
// -----------------------------------------------------------------------------
void Graphics::drawCircle(int16_t x, int16_t y,
                          int16_t r, Color color)
{
    GfxCirclePayload p{x, y, r, color};
    sendCommand(GFX_CMD_DRAW_CIRCLE, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// fillCircle()
// -----------------------------------------------------------------------------
void Graphics::fillCircle(int16_t x, int16_t y,
                          int16_t r, Color color)
{
    GfxCirclePayload p{x, y, r, color};
    sendCommand(GFX_CMD_FILL_CIRCLE, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawTriangle()
// -----------------------------------------------------------------------------
void Graphics::drawTriangle(int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2,
                            Color color)
{
    GfxTrianglePayload p{x0, y0, x1, y1, x2, y2, color};
    sendCommand(GFX_CMD_DRAW_TRIANGLE, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// fillTriangle()
// -----------------------------------------------------------------------------
void Graphics::fillTriangle(int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2,
                            Color color)
{
    GfxTrianglePayload p{x0, y0, x1, y1, x2, y2, color};
    sendCommand(GFX_CMD_FILL_TRIANGLE, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// drawBitmap() — 1-bit monochrome (Adafruit_GFX compatible)
// Payload = fixed fields + bitmap bytes
// -----------------------------------------------------------------------------
void Graphics::drawBitmap(int16_t x, int16_t y,
                          const uint8_t *bitmap,
                          int16_t w, int16_t h,
                          Color color)
{
    (void)color; // color is applied on iPhone side; bitmap is 1-bit mask
    GfxBitmapPayload p{x, y, w, h};

    // 1-bit bitmap size in bytes, rounded up
    uint16_t nBytes = (uint16_t)(((int32_t)w * (int32_t)h + 7) / 8);

    sendCommandWithTail(GFX_CMD_DRAW_BITMAP, &p, sizeof(p), bitmap, nBytes);
}

void Graphics::drawBitmap(int16_t x, int16_t y,
                          const uint8_t *bitmap,
                          int16_t w, int16_t h,
                          Color fg, Color bg)
{
    GfxBitmapBgPayload p{x, y, w, h, fg, bg};
    uint16_t nBytes = (uint16_t)(((int32_t)w * (int32_t)h + 7) / 8);

    sendCommandWithTail(GFX_CMD_DRAW_BITMAP_BG, &p, sizeof(p), bitmap, nBytes);
}

// -----------------------------------------------------------------------------
// Text API
// -----------------------------------------------------------------------------
void Graphics::setCursor(int16_t x, int16_t y)
{
    _cursorX = x;
    _cursorY = y;
    GfxSetCursorPayload p{x, y};
    sendCommand(GFX_CMD_SET_CURSOR, &p, sizeof(p));
}

void Graphics::setTextColor(Color color)
{
    _textColor = color;
    GfxSetTextColorPayload p{color};
    sendCommand(GFX_CMD_SET_TEXT_COLOR, &p, sizeof(p));
}

void Graphics::setTextColor(Color fg, Color bg)
{
    _textColor = fg;
    _textBgColor = bg;
    GfxSetTextColorBgPayload p{fg, bg};
    sendCommand(GFX_CMD_SET_TEXT_COLOR_BG, &p, sizeof(p));
}

void Graphics::setTextSize(uint8_t size)
{
    _textSize = size;
    GfxSetTextSizePayload p{size};
    sendCommand(GFX_CMD_SET_TEXT_SIZE, &p, sizeof(p));
}

void Graphics::setTextWrap(bool wrap)
{
    _wrap = wrap;
    GfxSetTextWrapPayload p{ static_cast<uint8_t>(wrap ? 1 : 0) };
    sendCommand(GFX_CMD_SET_TEXT_WRAP, &p, sizeof(p));
}

void Graphics::cp437(bool enable)
{
    _cp437 = enable;
    GfxCp437Payload p{ static_cast<uint8_t>(enable ? 1 : 0) };
    sendCommand(GFX_CMD_CP437, &p, sizeof(p));
}


void Graphics::setFont(const GFXfont *font)
{
    _font = font;

    // Placeholder mapping (you can expand this later)
    uint16_t id = font ? 1 : 0;
    GfxSetFontPayload p{id};
    sendCommand(GFX_CMD_SET_FONT, &p, sizeof(p));
}

// -----------------------------------------------------------------------------
// write() — core Print override
// -----------------------------------------------------------------------------
size_t Graphics::write(uint8_t c)
{
    GfxWriteCharPayload p{c};
    sendCommand(GFX_CMD_WRITE_CHAR, &p, sizeof(p));
    return 1;
}

// -----------------------------------------------------------------------------
// flush()
// -----------------------------------------------------------------------------
void Graphics::flush()
{
    sendCommand(GFX_CMD_FLUSH, nullptr, 0);
}
