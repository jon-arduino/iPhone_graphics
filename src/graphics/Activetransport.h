#pragma once

#include "graphics/GraphicsTransport.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ActiveTransport
//  Single-slot transport switcher. Holds a pointer to whichever transport
//  is currently active (BLE or WiFi). Graphics renders through this — one
//  pipeline regardless of which transport is connected.
//
//  Usage in main.cpp:
//    ActiveTransport  transport;
//    transport.set(&bleTransport);   // when BLE connects
//    transport.set(&wifiTransport);  // when WiFi connects
//    transport.set(nullptr);         // when disconnected
//
//  canSend() returns true only when an active transport is set AND ready.
//  switch() is safe to call at any time including mid-render — the current
//  send() will complete to whichever transport was active when it started.
// ─────────────────────────────────────────────────────────────────────────────
class ActiveTransport : public GraphicsTransport
{
public:
    ActiveTransport() = default;

    // Set the active transport. Pass nullptr to clear.
    void set(GraphicsTransport *t)
    {
        _active = t;
    }

    GraphicsTransport *get() const { return _active; }

    // GraphicsTransport interface
    void begin() override
    {
        if (_active)
            _active->begin();
    }

    bool canSend() const override
    {
        return _active != nullptr && _active->canSend();
    }

    void send(const uint8_t *data, uint16_t len) override
    {
        if (_active && _active->canSend())
            _active->send(data, len);
    }

    void flush() override
    {
        if (_active && _active->canSend())
            _active->flush();
    }

private:
    GraphicsTransport *_active = nullptr;
};
