#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "TelemetryPacket.h"

// Nordic UART Service-style UUIDs
static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *TX_CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // ESP32 -> iPhone (Notify)
static const char *RX_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // iPhone -> ESP32 (Write)

class BLEManager
{
public:
    BLEManager();

    void begin();

    // Ready to stream: connected + subscribed + characteristic exists
    bool canSend() const;

    // Send arbitrary bytes (used by BleGraphicsTransport / Graphics)
    void sendBytes(const uint8_t *data, uint16_t len);

    // Existing API
    void sendTelemetry(const TelemetryPacket &pkt);

    bool hasRxData() const;
    size_t readRx(uint8_t *dst, size_t maxLen);

private:
    // Callback helper types (defined in .cpp)
    class ServerCB;
    class TxCharCB;
    class RxCharCB;

    friend class ServerCB;
    friend class TxCharCB;
    friend class RxCharCB;

    // NimBLE objects
    NimBLEServer *pServer = nullptr;
    NimBLECharacteristic *pTxChar = nullptr;
    NimBLECharacteristic *pRxChar = nullptr;

    // State
    volatile bool _connected = false;
    volatile bool _notifySubscribed = false;
    volatile uint16_t _mtu = 23;

    // RX buffer
    static constexpr size_t RX_BUF_SIZE = 512;
    uint8_t rxBuf[RX_BUF_SIZE] = {0};
    volatile size_t rxLen = 0;

    // Helpers
    uint16_t effectiveChunkSize() const;
    void startAdvertising();
};
