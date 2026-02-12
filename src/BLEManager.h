/*  old incluces from BLEManager.h:
#pragma once
#include "TelemetryPacket.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLE2904.h>
#include "BleGraphicsTransport.h"  */

#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "TelemetryPacket.h"

// Nordic UART Service-style UUIDs (you’re already using these)
static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *TX_CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // ESP32 -> iPhone (Notify)
static const char *RX_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // iPhone -> ESP32 (Write)

class BLEManager
{
public:
    BLEManager();

    void begin();

    // Your existing API
    void sendTelemetry(const TelemetryPacket &pkt);

    // Optional: get last RX bytes from iPhone (if you use RX characteristic)
    bool hasRxData() const;
    size_t readRx(uint8_t *dst, size_t maxLen);

private:
    NimBLEServer *pServer;
    NimBLECharacteristic *pTxChar; // notify/read
    NimBLECharacteristic *pRxChar; // write

    volatile bool clientSubscribed;

    // Simple RX buffer
    static constexpr size_t RX_BUF_SIZE = 512;
    uint8_t rxBuf[RX_BUF_SIZE];
    volatile size_t rxLen;
};
