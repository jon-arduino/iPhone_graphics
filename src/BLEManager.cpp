#include "BLEManager.h"

static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";

BLEManager::BLEManager()
    : pServer(nullptr), pTelemetryChar(nullptr) {}

void BLEManager::begin()
{
    NimBLEDevice::init("ESP32-Telemetry");
    
    auto adv = NimBLEDevice::getAdvertising();
    adv->setName("ESP32-Telemetry");  // <-- This makes iOS show the name in the scan list
    // NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Optional but helpful (max power)

    // 🔍 Add this right here
    Serial.print("BLE MAC: ");
    Serial.println(NimBLEDevice::getAddress().toString().c_str());

    pServer = NimBLEDevice::createServer();

    NimBLEService *service = pServer->createService(SERVICE_UUID);

    pTelemetryChar = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::NOTIFY);

    service->start();
    NimBLEDevice::startAdvertising();

    Serial.println("BLE advertising started");
}

void BLEManager::sendTelemetry(const TelemetryPacket &pkt)
{
    if (!pTelemetryChar)
        return;

    pTelemetryChar->setValue((uint8_t *)&pkt, sizeof(pkt));
    pTelemetryChar->notify();
}