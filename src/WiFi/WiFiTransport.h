#pragma once

#include "graphics/GraphicsTransport.h"
#include "WiFiManager.h"
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Auto-flush timing constant — visible and adjustable from main.cpp:
//
//    WiFiTransport wifiTransport(wifiManager);
//    wifiTransport.setAutoFlushMs(WIFI_AUTO_FLUSH_MS);
//
//  If no explicit flush() arrives within this many milliseconds of the last
//  send(), the accumulated buffer is sent automatically. This catches graphics
//  code that never calls flush() and ensures nothing gets stuck.
//
//  Guidelines:
//    > ping interval (3000ms)  — never, would starve heartbeat
//    < frame render time       — never, would split frames mid-draw
//    50ms                      — good default: imperceptible lag, safe margin
// ─────────────────────────────────────────────────────────────────────────────

class WiFiTransport : public GraphicsTransport
{
public:
    explicit WiFiTransport(WiFiManager &wifi);

    void begin() override;
    bool canSend() const override;

    // Accumulate bytes into local buffer — do NOT send immediately
    void send(const uint8_t *data, uint16_t len) override;

    // Drain accumulated buffer as one TCP write (explicit frame boundary)
    void flush() override;

    // Set auto-flush timeout in milliseconds (call from main/setup)
    void setAutoFlushMs(uint32_t ms) { _autoFlushMs = ms; }

    // Call from loop() — triggers auto-flush if buffer has been sitting too long
    void tick();

private:
    WiFiManager &_wifi;
    std::vector<uint8_t> _txBuf;
    uint32_t _lastSendMs = 0;
    uint32_t _autoFlushMs = 50; // default, override via setAutoFlushMs()

    static constexpr size_t TX_BUF_RESERVE = 4096;
};