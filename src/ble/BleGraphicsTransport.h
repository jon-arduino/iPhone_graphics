#pragma once

#include <stdint.h>
#include "graphics/Graphics.h"
#include "BLEManager.h"

// Adapter: GraphicsTransport implemented over BLEManager (single BLE owner).
class BleGraphicsTransport : public GraphicsTransport
{
public:
    explicit BleGraphicsTransport(BLEManager &ble)
        : _ble(ble) {}

    void begin() override { _ble.begin(); }

    bool canSend() const { return _ble.canSend(); }

    void send(const uint8_t *data, uint16_t len) override
    {
        _ble.sendBytes(data, len);
    }

private:
    BLEManager &_ble;
};
