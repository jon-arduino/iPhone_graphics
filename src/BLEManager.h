#pragma once
#include "TelemetryPacket.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLE2904.h>
#include "BleGraphicsTransport.h"

class BLEManager
{
public:
    BLEManager();
    void begin();
    void sendTelemetry(const TelemetryPacket &pkt);

private:
    NimBLEServer *pServer;
    NimBLECharacteristic *pTelemetryChar;
};