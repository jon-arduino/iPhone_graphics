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

    // Optional
    GFX_CMD_GET_TEXT_BOUNDS = 0x60
};

// -----------------------------------------------------------------------------
// PACKING (CRITICAL FOR BLE PROTOCOL)
// -----------------------------------------------------------------------------
#if defined(__GNUC__)
#define GFX_PACKED __attribute__((packed))
#else
#pragma pack(push, 1)
#define GFX_PACKED
#endif

// -----------------------------------------------------------------------------
// Packet Header (3 bytes exactly)
// -----------------------------------------------------------------------------
struct GFX_PACKED GfxPacketHeader
{
    uint8_t cmd;     // GfxCommand
    uint16_t length; // payload length (little-endian)
};

// -----------------------------------------------------------------------------
// Payload Structures
// -----------------------------------------------------------------------------

// BEGIN
struct GFX_PACKED GfxBeginPayload
{
    uint16_t width;
    uint16_t height;
};

// CLEAR
struct GFX_PACKED GfxClearPayload
{
    uint16_t color; // RGB565
};

// DRAW_PIXEL
struct GFX_PACKED GfxDrawPixelPayload
{
    int16_t x;
    int16_t y;
    uint16_t color;
};

// DRAW_LINE
struct GFX_PACKED GfxDrawLinePayload
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint16_t color;
};

// FAST LINE
struct GFX_PACKED GfxFastLinePayload
{
    int16_t x;
    int16_t y;
    int16_t length;
    uint16_t color;
};

// RECT
struct GFX_PACKED GfxRectPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t color;
};

// ROUND RECT
struct GFX_PACKED GfxRoundRectPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t r;
    uint16_t color;
};

// CIRCLE
struct GFX_PACKED GfxCirclePayload
{
    int16_t x;
    int16_t y;
    int16_t r;
    uint16_t color;
};

// TRIANGLE
struct GFX_PACKED GfxTrianglePayload
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
struct GFX_PACKED GfxBitmapPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    // bitmap bytes follow immediately
};

// BITMAP with background
struct GFX_PACKED GfxBitmapBgPayload
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t fg;
    uint16_t bg;
    // bitmap bytes follow immediately
};

// TEXT
struct GFX_PACKED GfxSetCursorPayload
{
    int16_t x;
    int16_t y;
};

struct GFX_PACKED GfxSetTextColorPayload
{
    uint16_t color;
};

struct GFX_PACKED GfxSetTextColorBgPayload
{
    uint16_t fg;
    uint16_t bg;
};

struct GFX_PACKED GfxSetTextSizePayload
{
    uint8_t size;
};

struct GFX_PACKED GfxSetTextWrapPayload
{
    uint8_t wrap;
};

struct GFX_PACKED GfxCp437Payload
{
    uint8_t enable;
};

struct GFX_PACKED GfxSetFontPayload
{
    uint16_t fontId;
};

struct GFX_PACKED GfxWriteCharPayload
{
    uint8_t c;
};

struct GFX_PACKED GfxGetTextBoundsPayload
{
    int16_t x;
    int16_t y;
    // text bytes follow
};

#if !defined(__GNUC__)
#pragma pack(pop)
#endif
