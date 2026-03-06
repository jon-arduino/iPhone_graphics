#pragma once
#include "graphics/GraphicsTransport.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ActiveTransport — runtime transport switcher
//
//  Holds a pointer to whichever concrete transport is currently active
//  (BLE or WiFi). The entire graphics pipeline renders through this single
//  object — nothing above this layer needs to know which link is in use.
//
//  Usage:
//    ActiveTransport transport;
//    transport.set(&bleTransport);    // BLE connected
//    transport.set(&wifiTransport);   // WiFi connected (takes priority)
//    transport.set(nullptr);          // disconnected
//
//  All GraphicsTransport calls forward to the active transport.
//  All calls are safe when no transport is set — they become no-ops.
// ─────────────────────────────────────────────────────────────────────────────

class ActiveTransport : public GraphicsTransport
{
public:
    ActiveTransport() = default;

    // Set or clear the active transport. Safe to call at any time.
    void set(GraphicsTransport *t) { _active = t; }

    // Returns the currently active transport, or nullptr if none.
    GraphicsTransport *get() const { return _active; }

    // ── GraphicsTransport interface ───────────────────────────────────────────

    // Forwards to active transport — no-op if none set.
    void begin() override
    {
        if (_active)
            _active->begin();
    }

    // True only when a transport is set AND its link is ready.
    bool canSend() const override
    {
        return _active != nullptr && _active->canSend();
    }

    // Forward bytes — no-op if no transport or link is down.
    void send(const uint8_t *data, uint16_t len) override
    {
        if (_active && _active->canSend())
            _active->send(data, len);
    }

    // Forward flush to active transport (no-op if transport doesn't buffer).
    void flush() override
    {
        if (_active)
            _active->flush();
    }

    // Forward reset to active transport (no-op if transport doesn't buffer).
    void reset() override
    {
        if (_active)
            _active->reset();
    }

private:
    GraphicsTransport *_active = nullptr;
};