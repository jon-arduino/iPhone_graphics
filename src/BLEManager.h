#pragma once
#include <NimBLEDevice.h>
#include "TelemetryPacket.h"

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