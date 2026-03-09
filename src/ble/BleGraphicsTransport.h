#pragma once
#include "graphics/GraphicsTransport.h"
#include "BLEManager.h"

// ─────────────────────────────────────────────────────────────────────────────
//  BleGraphicsTransport — GraphicsTransport adapter for BLE
//
//  Coalesces small GFX draw calls into a static buffer before sending.
//  Each drawing primitive encodes to a small fixed payload (3-20 bytes).
//  Without coalescing, drawCircle would trigger 300+ individual BLE sends,
//  exhausting the NimBLE mbuf pool. With a 2KB buffer, the entire circle
//  accumulates before any BLE send occurs.
//
//  Buffer sizing (AUTO_FLUSH_BYTES):
//    - All primitives except drawBitmap are ≤20 bytes per send() call,
//      so a 2KB buffer is safe as a static array with no overflow risk.
//    - drawBitmap with a large bitmap could exceed 2KB in a single send()
//      call — guarded by an assert in send().
//    - 2KB = ~8 MTU chunks. BLEManager::sendBytes() splits and paces them.
//
//  Memory: 2KB in BSS (static). No heap allocation, no fragmentation.
//
//  Send flow:
//    send()  → append to _buf, auto-flush when _bufLen >= AUTO_FLUSH_BYTES
//    flush() → sendBytes(_buf, _bufLen), reset _bufLen = 0
//    reset() → _bufLen = 0, no send (called on disconnect)
// ─────────────────────────────────────────────────────────────────────────────

static constexpr uint16_t AUTO_FLUSH_BYTES = 4096;

class BleGraphicsTransport : public GraphicsTransport
{
public:
    explicit BleGraphicsTransport(BLEManager &ble) : _ble(ble), _bufLen(0) {}

    void begin() override { _ble.begin(); }
    bool canSend() const override { return _ble.canSend(); }

    void send(const uint8_t *data, uint16_t len) override
    {
        if (!canSend() || !data || len == 0)
            return;

        // Guard: a single send() call must never exceed the buffer.
        // All GFX primitives are ≤20 bytes. Only drawBitmap could be large.
        configASSERT(len <= AUTO_FLUSH_BYTES);

        if (_bufLen + len > AUTO_FLUSH_BYTES)
            flush();
        memcpy(_buf + _bufLen, data, len);
        _bufLen += len;
        if (_bufLen >= AUTO_FLUSH_BYTES)
            flush();
    }

    void flush() override
    {
        if (_bufLen == 0)
            return;
        if (canSend())
            _ble.sendBytes(_buf, _bufLen);
        _bufLen = 0;
    }

    void reset() override { _bufLen = 0; }

private:
    BLEManager &_ble;
    uint8_t _buf[AUTO_FLUSH_BYTES];
    uint16_t _bufLen;
};