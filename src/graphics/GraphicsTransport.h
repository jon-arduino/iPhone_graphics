#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
//  GraphicsTransport — abstract base for all display transports
//
//  Defines the contract that every transport must fulfil so the graphics
//  pipeline (Graphics, Adafruit_iPhoneTFT, ActiveTransport) can work with
//  any physical link — BLE, WiFi, UART, USB, or a test mock — identically.
//
//  REQUIRED (pure virtual — subclass will not compile without these):
//    begin()    — one-time hardware/stack initialisation
//    send()     — deliver raw encoded bytes to the remote display
//    canSend()  — true when the link is up and ready to accept data
//
//  OPTIONAL (virtual no-ops — override only if the transport buffers data):
//    flush()    — push any accumulated buffer to the wire at a frame boundary.
//                 Called by Graphics at logical end-of-frame. Transports that
//                 send immediately in send() can leave this as a no-op.
//    reset()    — discard any buffered bytes without sending. Called on
//                 disconnect to prevent stale data reaching a new session.
//                 Transports with no internal buffer can leave this as a no-op.
//
//  Adding a new transport:
//    1. Inherit from GraphicsTransport
//    2. Implement begin(), send(), canSend()
//    3. Override flush() and reset() if your transport buffers internally
// ─────────────────────────────────────────────────────────────────────────────

class GraphicsTransport
{
public:
    virtual ~GraphicsTransport() = default;

    // ── Required ─────────────────────────────────────────────────────────────

    // One-time initialisation — called once from setup().
    virtual void begin() = 0;

    // Deliver bytes to the remote display. May buffer internally.
    // Called repeatedly for each encoded GFX command fragment.
    virtual void send(const uint8_t *data, uint16_t len) = 0;

    // Returns true when the link is established and ready to accept data.
    // send() and flush() are no-ops when canSend() is false.
    virtual bool canSend() const = 0;

    // ── Optional ─────────────────────────────────────────────────────────────

    // Flush any internally buffered bytes to the wire.
    // Called at logical frame boundaries (end of renderTelemetry, GFX scene, etc.)
    // Transports that send immediately in send() may leave this as a no-op.
    virtual void flush() {}

    // Discard any internally buffered bytes without sending.
    // Called on disconnect to prevent stale data reaching a new connection.
    // Transports with no internal buffer may leave this as a no-op.
    virtual void reset() {}
};