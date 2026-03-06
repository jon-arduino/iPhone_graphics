#pragma once
#include <vector>
#include "graphics/GraphicsTransport.h"
#include "WiFiManager.h"

// ─────────────────────────────────────────────────────────────────────────────
//  WiFiTransport — GraphicsTransport adapter for TCP/WiFi
//
//  Sits between the graphics pipeline and WiFiManager. Accumulates GFX bytes
//  into an internal buffer and sends them as a single TCP write on flush().
//  This batching is important — many small TCP writes cause head-of-line
//  blocking and Nagle algorithm delays on iOS.
//
//  Send flow:
//    send()  → accumulate into _txBuf, reset auto-flush timer
//    flush() → send entire _txBuf as one WiFiManager::send() call
//              called explicitly at frame boundaries
//    tick()  → call from loop() — triggers auto-flush if bytes have been
//              sitting idle longer than _autoFlushMs. Catches code paths
//              that never call flush() explicitly.
//    reset() → discard _txBuf without sending — called on disconnect
//              to prevent stale bytes reaching the next connection's stream
//
//  Auto-flush timing:
//    Set via setAutoFlushMs() from setup(). Default 50ms.
//    Must be: > single frame render time (~10ms)
//             < ping interval (3000ms)
//    Mirror GFXAutoFlushIntervalSeconds on the iPhone side.
// ─────────────────────────────────────────────────────────────────────────────

class WiFiTransport : public GraphicsTransport
{
public:
    explicit WiFiTransport(WiFiManager &wifi);

    // Initialise internal buffer — stack init handled by WiFiManager::begin().
    void begin() override;

    // True when WiFi is connected and a TCP client is attached.
    bool canSend() const override;

    // Accumulate bytes — nothing sent to TCP until flush() or auto-flush.
    void send(const uint8_t *data, uint16_t len) override;

    // Send entire accumulated buffer as one TCP write.
    // No-op if buffer is empty. Discards buffer if link is down.
    void flush() override;

    // Discard buffered bytes without sending.
    // Call on disconnect — prevents stale frame data reaching next session.
    void reset() override;

    // Call from loop() every iteration — triggers auto-flush on idle timeout.
    void tick();

    // Configure auto-flush timeout. Call from setup() after construction.
    void setAutoFlushMs(uint32_t ms) { _autoFlushMs = ms; }

private:
    WiFiManager &_wifi;
    std::vector<uint8_t> _txBuf;
    uint32_t _lastSendMs = 0;
    uint32_t _autoFlushMs = 50;

    static constexpr size_t TX_BUF_RESERVE = 4096;
};