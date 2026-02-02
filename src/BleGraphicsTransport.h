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

    // NimBLEServerCallbacks — 2.1.0 signatures
    void onConnect(NimBLEServer *pServer) override;
    void onDisconnect(NimBLEServer *pServer) override;
    void onMTUChange(uint16_t mtu, NimBLEConnInfo &connInfo) override;

    // NimBLECharacteristicCallbacks — 2.1.0 signature
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