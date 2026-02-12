#include "BLEManager.h"

class ServerCallbacks : public NimBLEServerCallbacks
{
public:
    explicit ServerCallbacks(volatile bool *subscribedFlag)
        : subscribed(subscribedFlag) {}

    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
        Serial.println("[BLE] Client connected");
        Serial.print("[BLE] Peer: ");
        Serial.println(connInfo.getAddress().toString().c_str());
        // Keep advertising off while connected (default behavior is fine)
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        Serial.print("[BLE] Client disconnected, reason=");
        Serial.println(reason);

        // Reset subscription state (iPhone must re-subscribe after reconnect)
        if (subscribed)
            *subscribed = false;

        // Restart advertising so iPhone can see us again
        NimBLEDevice::startAdvertising();
        Serial.println("[BLE] Advertising restarted");
    }

private:
    volatile bool *subscribed;
};

class TxCallbacks : public NimBLECharacteristicCallbacks
{
public:
    explicit TxCallbacks(volatile bool *subscribedFlag)
        : subscribed(subscribedFlag) {}

    void onSubscribe(NimBLECharacteristic *pCharacteristic,
                     NimBLEConnInfo &connInfo,
                     uint16_t subValue) override
    {
        bool isSub = (subValue != 0);
        if (subscribed)
            *subscribed = isSub;

        Serial.print("[BLE] TX subscribed=");
        Serial.println(isSub ? "YES" : "NO");
    }

private:
    volatile bool *subscribed;
};

class RxCallbacks : public NimBLECharacteristicCallbacks
{
public:
    explicit RxCallbacks(uint8_t *buf, volatile size_t *len, size_t cap)
        : rxBuf(buf), rxLen(len), capacity(cap) {}

    void onWrite(NimBLECharacteristic *pCharacteristic,
                 NimBLEConnInfo &connInfo) override
    {
        std::string v = pCharacteristic->getValue();
        size_t n = v.size();
        if (n > capacity)
            n = capacity;

        memcpy(rxBuf, v.data(), n);
        *rxLen = n;

        Serial.print("[BLE] RX write ");
        Serial.print(n);
        Serial.println(" bytes");
    }

private:
    uint8_t *rxBuf;
    volatile size_t *rxLen;
    size_t capacity;
};

// ---- BLEManager implementation ----

BLEManager::BLEManager()
    : pServer(nullptr),
      pTxChar(nullptr),
      pRxChar(nullptr),
      clientSubscribed(false),
      rxLen(0)
{
    memset(rxBuf, 0, sizeof(rxBuf));
}

void BLEManager::begin()
{
    NimBLEDevice::init("ESP32-Telemetry");

    // Debug-friendly power
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    Serial.print("[BLE] MAC: ");
    Serial.println(NimBLEDevice::getAddress().toString().c_str());

    // Optional: larger MTU helps if you ever send > 20 bytes per update
    // Must be set before connections are made.
    // NimBLEDevice::setMTU(185);

    pServer = NimBLEDevice::createServer();

    static ServerCallbacks serverCB(&clientSubscribed);
    pServer->setCallbacks(&serverCB);

    NimBLEService *service = pServer->createService(NimBLEUUID(SERVICE_UUID));

    // TX: ESP32 -> iPhone (Notify + Read)
    pTxChar = service->createCharacteristic(
        NimBLEUUID(TX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    static TxCallbacks txCB(&clientSubscribed);
    pTxChar->setCallbacks(&txCB);

    // Optional: set an initial value so iPhone "read" works immediately
    uint8_t hello[] = {0x01};
    pTxChar->setValue(hello, sizeof(hello));

    // RX: iPhone -> ESP32 (Write)
    pRxChar = service->createCharacteristic(
        NimBLEUUID(RX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

    static RxCallbacks rxCB(rxBuf, &rxLen, RX_BUF_SIZE);
    pRxChar->setCallbacks(&rxCB);

    service->start();

    // ---- Advertising: iOS filtered scan REQUIRES service UUID in PRIMARY ADV ----
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->reset();

    // Primary ADV: small + guaranteed UUID
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.addServiceUUID(NimBLEUUID(SERVICE_UUID));
    adv->setAdvertisementData(advData);

    // Scan response: put the name here (keeps primary packet small)
    NimBLEAdvertisementData scanResp;
    scanResp.setName("ESP32-Telemetry");
    adv->setScanResponseData(scanResp);

    // Reasonable intervals for reliability (0.625ms units)
    adv->setMinInterval(48);  // 30ms
    adv->setMaxInterval(160); // 100ms

    adv->start();
    Serial.println("[BLE] Advertising started (service UUID in primary ADV)");
}

void BLEManager::sendTelemetry(const TelemetryPacket &pkt)
{
    if (!pTxChar)
        return;

    // Only notify if the iPhone has subscribed
    if (!clientSubscribed)
        return;

    pTxChar->setValue((uint8_t *)&pkt, sizeof(pkt));
    pTxChar->notify();
}

bool BLEManager::hasRxData() const
{
    return rxLen > 0;
}

size_t BLEManager::readRx(uint8_t *dst, size_t maxLen)
{
    size_t n = rxLen;
    if (n == 0)
        return 0;
    if (n > maxLen)
        n = maxLen;

    memcpy(dst, rxBuf, n);
    rxLen = 0; // consume
    return n;
}
