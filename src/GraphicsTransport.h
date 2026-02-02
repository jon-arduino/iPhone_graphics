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
    virtual ~GraphicsTransport() {}

    // Called once at startup (BLE init, Wi-Fi connect, etc.)
    virtual void begin() = 0;

    // Send raw bytes over the transport.
    // The Graphics encoder guarantees that 'data' is a complete packet fragment.
    virtual void send(const uint8_t *data, uint16_t len) = 0;
};