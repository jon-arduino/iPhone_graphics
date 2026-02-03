#pragma once
#include <stdint.h>
#include <NimBLEDevice.h>
#include <NimBLE2904.h>
#include "GraphicsTransport.h"

class BleGraphicsTransport : public GraphicsTransport,
                             public NimBLEServerCallbacks,
                             public NimBLECharacteristicCallbacks
{
public:
    BleGraphicsTransport(const char *deviceName,
                         const char *serviceUUID,
                         const char *characteristicUUID);

    void begin() override;
    void send(const uint8_t *data, uint16_t len) override;

    bool isConnected() const { return _connected; }
    uint16_t negotiatedMTU() const { return _mtu; }

    // NimBLEServerCallbacks — must match NimBLEServerCallbacks exactly
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;
    void onMTUChange(uint16_t mtu, NimBLEConnInfo &connInfo) override;

    // NimBLECharacteristicCallbacks — NimBLEConnInfo-based signature
    void onSubscribe(NimBLECharacteristic *pCharacteristic,
                     NimBLEConnInfo &connInfo,
                     uint16_t subValue) override;

private:
    const char *_deviceName;
    const char *_serviceUUID;
    const char *_charUUID;

    NimBLEServer *_server = nullptr;
    NimBLECharacteristic *_characteristic = nullptr;

    bool _connected = false;
    bool _notificationsEnabled = false;
    uint16_t _mtu = 185;

    uint16_t effectiveChunkSize() const;
    bool canSend() const;
};