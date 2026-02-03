#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLE2904.h>
#include "BleGraphicsTransport.h"

BleGraphicsTransport::BleGraphicsTransport(const char *deviceName,
                                           const char *serviceUUID,
                                           const char *characteristicUUID)
    : _deviceName(deviceName),
      _serviceUUID(serviceUUID),
      _charUUID(characteristicUUID) {}

uint16_t BleGraphicsTransport::effectiveChunkSize() const
{
    uint16_t maxPayload = (_mtu > 23) ? (_mtu - 3) : 20;
    if (maxPayload > 180)
        maxPayload = 180;
    return maxPayload;
}

bool BleGraphicsTransport::canSend() const
{
    return _connected && _notificationsEnabled && _characteristic != nullptr;
}

void BleGraphicsTransport::begin()
{
    NimBLEDevice::setMTU(_mtu);
    NimBLEDevice::init(_deviceName);

    _server = NimBLEDevice::createServer();
    _server->setCallbacks(this);

    NimBLEService *service = _server->createService(_serviceUUID);

    _characteristic = service->createCharacteristic(
        _charUUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);

    _characteristic->setCallbacks(this);
    _characteristic->addDescriptor(new NimBLE2904());

    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(_serviceUUID);

    // Your version supports setScanResponseData(const NimBLEAdvertisementData&)
    NimBLEAdvertisementData scanResp;
    advertising->setScanResponseData(scanResp);

    advertising->start();
}

// ------------------ NimBLEServerCallbacks ------------------

void BleGraphicsTransport::onConnect(NimBLEServer *pServer,
                                     NimBLEConnInfo &connInfo)
{
    _connected = true;
}

void BleGraphicsTransport::onDisconnect(NimBLEServer *pServer,
                                        NimBLEConnInfo &connInfo,
                                        int reason)
{
    _connected = false;
    _notificationsEnabled = false;
    NimBLEDevice::startAdvertising();
}

void BleGraphicsTransport::onMTUChange(uint16_t mtu,
                                       NimBLEConnInfo &connInfo)
{
    _mtu = mtu;
}

// ------------------ NimBLECharacteristicCallbacks ------------------

void BleGraphicsTransport::onSubscribe(NimBLECharacteristic *pCharacteristic,
                                       NimBLEConnInfo &connInfo,
                                       uint16_t subValue)
{
    _notificationsEnabled = (subValue & 0x0001) != 0;
}

// ------------------ send() ------------------

void BleGraphicsTransport::send(const uint8_t *data, uint16_t len)
{
    if (!canSend())
        return;

    uint16_t chunkSize = effectiveChunkSize();
    uint16_t offset = 0;

    while (offset < len)
    {
        if (!canSend())
            break;

        uint16_t chunk = (len - offset > chunkSize) ? chunkSize : (len - offset);

        _characteristic->setValue(data + offset, chunk);
        _characteristic->notify();

        offset += chunk;
        delay(2);
    }
}