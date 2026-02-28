#pragma once
#include <stdint.h>

// -----------------------------------------------------------------------------
// GraphicsTransport
// Abstract base class for sending encoded graphics packets over any transport.
// BLE, Wi-Fi, UART, USB, or even a mock transport for testing can implement this.
// -----------------------------------------------------------------------------
class GraphicsTransport
{
public:
    virtual ~GraphicsTransport() = default;

    // Called once at startup (BLE init, Wi-Fi connect, etc.)
    virtual void begin() = 0;

    // Send raw bytes over the transport.
    // The Graphics encoder guarantees that 'data' is a complete packet fragment.
    virtual void send(const uint8_t *data, uint16_t len) = 0;

    // Check if the transport can send data
    virtual bool canSend() const = 0;

    // Called at logical frame boundaries by Graphics::flush().
    // Default is no-op — transports that batch (WiFiTransport) override this
    // to drain their accumulation buffer as a single write.
    // BleGraphicsTransport leaves this as no-op (ring buffer handles batching).
    virtual void flush() {}
};