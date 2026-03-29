#include "Adafruit_iPhoneTFT.h"

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
Adafruit_iPhoneTFT::Adafruit_iPhoneTFT(Graphics &gfx, int16_t w, int16_t h)
    : Adafruit_GFX(w, h), _gfx(gfx), _rawW(w), _rawH(h)
{
    markAllTextDirty();
}

// -----------------------------------------------------------------------------
// begin()
// -----------------------------------------------------------------------------
void Adafruit_iPhoneTFT::begin(uint16_t clearColor)
{
    // Allocate remote surface
    _gfx.begin((uint16_t)_rawW, (uint16_t)_rawH);

    // Keep local state consistent
    Adafruit_GFX::setRotation(0);

    // Clear (optional)
    _gfx.clear(clearColor);

    // After BEGIN, remote state is effectively reset
    _remoteValid = false;
    markAllTextDirty();
}

void Adafruit_iPhoneTFT::flush()
{
    _gfx.flush();
}

// -----------------------------------------------------------------------------
// Transaction API (no batching opcode exists yet, but we can still override)
// -----------------------------------------------------------------------------
void Adafruit_iPhoneTFT::startWrite(void)
{
    // No-op for now (could add a future "BEGIN_BATCH" opcode)
}

void Adafruit_iPhoneTFT::endWrite(void)
{
    // No-op for now (could add a future "END_BATCH" opcode)
}

void Adafruit_iPhoneTFT::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    _gfx.drawPixel(x, y, color);
}

void Adafruit_iPhoneTFT::writePixel(int16_t x, int16_t y, uint16_t color)
{
    _gfx.drawPixel(x, y, color);
}

void Adafruit_iPhoneTFT::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (w <= 0 || h <= 0)
        return;
    _gfx.fillRect(x, y, w, h, color);
}

void Adafruit_iPhoneTFT::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (w <= 0)
        return;
    _gfx.drawFastHLine(x, y, w, color);
}

void Adafruit_iPhoneTFT::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (h <= 0)
        return;
    _gfx.drawFastVLine(x, y, h, color);
}

void Adafruit_iPhoneTFT::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    _gfx.drawLine(x0, y0, x1, y1, color);
}

// -----------------------------------------------------------------------------
// Control API
// -----------------------------------------------------------------------------
void Adafruit_iPhoneTFT::setRotation(uint8_t r)
{
    r &= 3;

    // Update local Adafruit_GFX bookkeeping (_width/_height, rotation, etc.)
    Adafruit_GFX::setRotation(r);

    // Tell phone (this is your protocol op)
    _gfx.setRotation(r);

    // Rotation affects wrapping math in Adafruit_GFX, but our phone cursor state
    // is in the same logical coords used by the sketch, so just mark dirty.
    markAllTextDirty();
}

void Adafruit_iPhoneTFT::invertDisplay(bool i)
{
    _gfx.invertDisplay(i);
    // No Adafruit_GFX internal state change needed beyond behavior
}

// -----------------------------------------------------------------------------
// Basic draw API (sketches call these directly often)
// -----------------------------------------------------------------------------
void Adafruit_iPhoneTFT::fillScreen(uint16_t color)
{
    _gfx.clear(color);
}

void Adafruit_iPhoneTFT::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    writeFastHLine(x, y, w, color);
}

void Adafruit_iPhoneTFT::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    writeFastVLine(x, y, h, color);
}

void Adafruit_iPhoneTFT::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    writeFillRect(x, y, w, h, color);
}

void Adafruit_iPhoneTFT::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    writeLine(x0, y0, x1, y1, color);
}

void Adafruit_iPhoneTFT::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (w <= 0 || h <= 0)
        return;
    _gfx.drawRect(x, y, w, h, color);
}

// -----------------------------------------------------------------------------
// Print override (fast text path)
// -----------------------------------------------------------------------------
size_t Adafruit_iPhoneTFT::write(uint8_t c)
{
    // If sketch used Adafruit_GFX base setters (e.g., via Adafruit_GFX&),
    // our local members changed but the phone didn't hear about it yet.
    // Sync before sending chars so phone renders exactly the same.
    syncTextStateIfNeeded();

    // Send the character opcode (your existing Graphics::write handles it)
    _gfx.write(c);

    // Maintain Adafruit_GFX cursor bookkeeping too.
    // Adafruit_GFX::write(uint8_t) would draw pixels; we don't want that.
    // But we DO want cursor_x/y to advance like Adafruit expects.
    //
    // Your iPhone renderer implements Adafruit classic font semantics:
    // - '\n': x=0, y += 8*textsize_y
    // - '\r': ignore
    // - otherwise: x += 6*textsize_x, with wrap if enabled
    //
    // So we mirror that logic locally:

    if (c == '\r')
        return 1;

    if (c == '\n')
    {
        cursor_x = 0;
        cursor_y += (int16_t)(8 * textsize_y);
        _dirtyCursor = true; // phone also moved cursor; keep consistent for next sync
        return 1;
    }

    int16_t charW = (int16_t)(6 * textsize_x);
    int16_t charH = (int16_t)(8 * textsize_y);

    if (wrap && (cursor_x + charW) > _width)
    {
        cursor_x = 0;
        cursor_y += charH;
    }
    cursor_x += charW;

    _dirtyCursor = true; // phone advanced too, but we still treat cursor as "changed"
    return 1;
}

// -----------------------------------------------------------------------------
// Hidden text setters (fast immediate state push when used on concrete type)
// -----------------------------------------------------------------------------
void Adafruit_iPhoneTFT::setCursor(int16_t x, int16_t y)
{
    Adafruit_GFX::setCursor(x, y);
    _dirtyCursor = true;
}

void Adafruit_iPhoneTFT::setTextColor(uint16_t c)
{
    Adafruit_GFX::setTextColor(c);
    _dirtyColor = true;
}

void Adafruit_iPhoneTFT::setTextColor(uint16_t c, uint16_t bg)
{
    Adafruit_GFX::setTextColor(c, bg);
    _dirtyColor = true;
}

void Adafruit_iPhoneTFT::setTextSize(uint8_t s)
{
    Adafruit_GFX::setTextSize(s);
    _dirtySize = true;
}

void Adafruit_iPhoneTFT::setTextSize(uint8_t sx, uint8_t sy)
{
    Adafruit_GFX::setTextSize(sx, sy);
    _dirtySize = true;
}

void Adafruit_iPhoneTFT::setTextWrap(bool w)
{
    Adafruit_GFX::setTextWrap(w);
    _dirtyWrap = true;
}

void Adafruit_iPhoneTFT::cp437(bool x)
{
    Adafruit_GFX::cp437(x);
    _dirtyCP437 = true;
}

// -----------------------------------------------------------------------------
// Dirty/sync helpers
// -----------------------------------------------------------------------------
void Adafruit_iPhoneTFT::markAllTextDirty()
{
    _dirtyCursor = true;
    _dirtyColor = true;
    _dirtySize = true;
    _dirtyWrap = true;
    _dirtyCP437 = true;
}

// This pushes Adafruit_GFX text state to the phone just-in-time.
void Adafruit_iPhoneTFT::syncTextStateIfNeeded()
{
    // If we've never synced since begin/rotation/etc, force full sync
    if (!_remoteValid)
    {
        _dirtyCursor = _dirtyColor = _dirtySize = _dirtyWrap = _dirtyCP437 = true;
    }

    // Cursor
    if (_dirtyCursor || !_remoteValid ||
        _rCursorX != cursor_x || _rCursorY != cursor_y)
    {
        _gfx.setCursor(cursor_x, cursor_y);
        _rCursorX = cursor_x;
        _rCursorY = cursor_y;
        _dirtyCursor = false;
    }

    // Color: Adafruit "transparent" text is represented by textcolor == textbgcolor.
    // Your protocol uses:
    //   setTextColor(fg)   (fg only)
    //   setTextColor(fg,bg) (fg+bg)
    //
    // So:
    // - if textcolor == textbgcolor => send fg-only
    // - else send fg+bg
    if (_dirtyColor || !_remoteValid ||
        _rTextColor != textcolor || _rTextBg != textbgcolor)
    {

        if (textcolor == textbgcolor)
        {
            _gfx.setTextColor(textcolor);
        }
        else
        {
            _gfx.setTextColor(textcolor, textbgcolor);
        }
        _rTextColor = textcolor;
        _rTextBg = textbgcolor;
        _dirtyColor = false;
    }

    // Size: your phone side currently supports a single scalar (and uses classic font)
    // We'll map Adafruit's X/Y to a single value if they match; if not, use X.
    if (_dirtySize || !_remoteValid ||
        _rTextSizeX != textsize_x || _rTextSizeY != textsize_y)
    {

        uint8_t s = textsize_x; // best effort
        if (textsize_y == textsize_x)
            s = textsize_x;

        _gfx.setTextSize(s);

        _rTextSizeX = textsize_x;
        _rTextSizeY = textsize_y;
        _dirtySize = false;
    }

    // Wrap
    if (_dirtyWrap || !_remoteValid || _rWrap != wrap)
    {
        _gfx.setTextWrap(wrap);
        _rWrap = wrap;
        _dirtyWrap = false;
    }

    // CP437
    if (_dirtyCP437 || !_remoteValid || _rCP437 != _cp437)
    {
        _gfx.cp437(_cp437);
        _rCP437 = _cp437;
        _dirtyCP437 = false;
    }

    _remoteValid = true;
}
