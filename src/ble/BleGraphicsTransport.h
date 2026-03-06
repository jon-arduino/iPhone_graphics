#pragma once
#include <vector>
#include "graphics/GraphicsTransport.h"
#include "BLEManager.h"

// ─────────────────────────────────────────────────────────────────────────────
//  BleGraphicsTransport — GraphicsTransport adapter for BLE
//
//  Sits between the graphics pipeline and BLEManager. Its primary job is
//  coalescing — GFX draw calls produce many small byte sequences (3-10 bytes
//  each). Sending each as a separate BLE notification wastes bandwidth and
//  exhausts NimBLE's mbuf pool. This transport accumulates writes into an
//  internal buffer and only calls BLEManager::sendBytes() at flush points.
//
//  Send flow:
//    send()  → accumulate into _buf
//              auto-flush when _buf reaches one full MTU chunk
//    flush() → send entire _buf as one BLEManager::sendBytes() call
//              called explicitly at frame boundaries (end of renderTelemetry,
//              end of GFX scene, etc.)
//    reset() → discard _buf without sending — called on disconnect
//
//  BLEManager::sendBytes() handles all BLE-level pacing internally:
//  one notification per ACK, semaphore-gated, with retry on error=6.
// ─────────────────────────────────────────────────────────────────────────────

class BleGraphicsTransport : public GraphicsTransport
{
public:
    explicit BleGraphicsTransport(BLEManager &ble) : _ble(ble) {}

    // Initialise the BLE stack — called once from setup().
    void begin() override { _ble.begin(); }

    // True when BLE is connected and iPhone has subscribed to TX characteristic.
    bool canSend() const override { return _ble.canSend(); }

    // Accumulate bytes into buffer. Auto-flushes when a full MTU chunk is ready.
    void send(const uint8_t *data, uint16_t len) override
    {
        if (!canSend() || !data || len == 0)
            return;
        _buf.insert(_buf.end(), data, data + len);
        if (_buf.size() >= _ble.effectiveChunkSize())
            flush();
    }

    // Send buffered bytes to BLEManager as one contiguous block.
    // BLEManager splits into MTU chunks and paces with ACK gating internally.
    void flush() override
    {
        if (_buf.empty())
            return;
        if (canSend())
            _ble.sendBytes(_buf.data(), (uint16_t)_buf.size());
        _buf.clear();
    }

    // Discard buffer without sending — called on disconnect.
    void reset() override { _buf.clear(); }

private:
    BLEManager &_ble;
    std::vector<uint8_t> _buf;
};