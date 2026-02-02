#pragma once
#include <stdint.h>

// -----------------------------------------------------------------------------
// Protocol Version
// -----------------------------------------------------------------------------
#define GFX_PROTOCOL_VERSION 1

// -----------------------------------------------------------------------------
// Command Identifiers
// -----------------------------------------------------------------------------
enum GfxCommand : uint8_t
{
    // System
    GFX_CMD_BEGIN = 0x01,
    GFX_CMD_CLEAR = 0x02,
    GFX_CMD_FLUSH = 0x03,

    // Pixels & Lines
    GFX_CMD_DRAW_PIXEL = 0x10,
    GFX_CMD_DRAW_LINE = 0x11,
    GFX_CMD_DRAW_FAST_HLINE = 0x12,
    GFX_CMD_DRAW_FAST_VLINE = 0x13,

    // Rectangles
    GFX_CMD_DRAW_RECT = 0x20,
    GFX_CMD_FILL_RECT = 0x21,
    GFX_CMD_DRAW_ROUNDRECT = 0x22,
    GFX_CMD_FILL_ROUNDRECT = 0x23,

    // Circles & Triangles
    GFX_CMD_DRAW_CIRCLE = 0x30,
    GFX_CMD_FILL_CIRCLE = 0x31,
    GFX_CMD_DRAW_TRIANGLE = 0x32,
    GFX_CMD_FILL_TRIANGLE = 0x33,

    // Bitmaps
    GFX_CMD_DRAW_BITMAP = 0x40,
    GFX_CMD_DRAW_BITMAP_BG = 0x41,

    // Text
    GFX_CMD_SET_CURSOR = 0x50,
    GFX_CMD_SET_TEXT_COLOR = 0x51,
    GFX_CMD_SET_TEXT_COLOR_BG = 0x52,
    GFX_CMD_SET_TEXT_SIZE = 0x53,
    GFX_CMD_SET_TEXT_WRAP = 0x54,
    GFX_CMD_CP437 = 0x55,
    GFX_CMD_SET_FONT = 0x56,
    GFX_CMD_WRITE_CHAR = 0x57,

    // Utility
    GFX_CMD_GET_TEXT_BOUNDS = 0x60 // (optional round‑trip)
};

// -----------------------------------------------------------------------------
// Packet Header
// -----------------------------------------------------------------------------
struct GfxPacketHeader
{
    uint8_t cmd;     // GfxCommand
    uint16_t length; // payload length in bytes (little-endian)
};

// -----------------------------------------------------------------------------
// Payload Structures
// -----------------------------------------------------------------------------

// BEGIN (width, height)
struct GfxBeginPayload
{
    uint16_t width;
    uint16_t height;
};

// CLEAR (color)
struct GfxClearPayload
{
    uint16_t color; // RGB565
};

// DRAW_PIXEL
struct GfxDrawPixelPayload
{
    int16_t x;
    int16_t y;
    uint16_t color;
};

// DRAW_LINE
struct GfxDrawLinePayload
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint16_t color;
};

// FAST HLINE / VLINE
struct GfxFastLinePayload
{
    int16_t x;
    int16_t y;
    int16_t length;
    uint16_t color;
};

// RECT / FILL_RECT
struct GfxRectPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t color;
};

// ROUND RECT
struct GfxRoundRectPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t r;
    uint16_t color;
};

// CIRCLE
struct GfxCirclePayload
{
    int16_t x;
    int16_t y;
    int16_t r;
    uint16_t color;
};

// TRIANGLE
struct GfxTrianglePayload
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    uint16_t color;
};

// BITMAP (monochrome)
struct GfxBitmapPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    // Followed by bitmap bytes
};

// BITMAP with background
struct GfxBitmapBgPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t fg;
    uint16_t bg;
    // Followed by bitmap bytes
};

// TEXT: SET_CURSOR
struct GfxSetCursorPayload
{
    int16_t x;
    int16_t y;
};

// TEXT: SET_TEXT_COLOR
struct GfxSetTextColorPayload
{
    uint16_t color;
};

// TEXT: SET_TEXT_COLOR_BG
struct GfxSetTextColorBgPayload
{
    uint16_t fg;
    uint16_t bg;
};

// TEXT: SET_TEXT_SIZE
struct GfxSetTextSizePayload
{
    uint8_t size;
};

// TEXT: SET_TEXT_WRAP
struct GfxSetTextWrapPayload
{
    uint8_t wrap; // 0 or 1
};

// TEXT: CP437
struct GfxCp437Payload
{
    uint8_t enable;
};

// TEXT: SET_FONT
struct GfxSetFontPayload
{
    uint16_t fontId; // iPhone maps this to a real font
};

// TEXT: WRITE_CHAR
struct GfxWriteCharPayload
{
    uint8_t c;
};

// GET_TEXT_BOUNDS (optional)
struct GfxGetTextBoundsPayload
{
    int16_t x;
    int16_t y;
    // Followed by text bytes
};