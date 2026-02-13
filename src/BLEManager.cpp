#include "BLEManager.h"
#include <NimBLE2904.h>
#include <string.h>

// -------------------- Callback Implementations --------------------

class BLEManager::ServerCB : public NimBLEServerCallbacks
{
public:
    explicit ServerCB(BLEManager *owner) : _owner(owner) {}

    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
        (void)pServer;
        _owner->_connected = true;
        Serial.println("[BLE] connected");
        Serial.print("[BLE] peer: ");
        Serial.println(connInfo.getAddress().toString().c_str());
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        (void)pServer;
        (void)connInfo;
        _owner->_connected = false;
        _owner->_notifySubscribed = false;
        Serial.print("[BLE] disconnected reason=");
        Serial.println(reason);
        _owner->startAdvertising();
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo &connInfo) override
    {
        (void)connInfo;
        _owner->_mtu = mtu;
        Serial.print("[BLE] MTU=");
        Serial.println(mtu);
    }

private:
    BLEManager *_owner;
};

class BLEManager::TxCharCB : public NimBLECharacteristicCallbacks
{
public:
    explicit TxCharCB(BLEManager *owner) : _owner(owner) {}

    void onSubscribe(NimBLECharacteristic *pCharacteristic,
                     NimBLEConnInfo &connInfo,
                     uint16_t subValue) override
    {
        (void)pCharacteristic;
        (void)connInfo;
        _owner->_notifySubscribed = (subValue & 0x0001) != 0;
        Serial.print("[BLE] notify subscribed=");
        Serial.println(_owner->_notifySubscribed ? "YES" : "NO");
    }

private:
    BLEManager *_owner;
};

class BLEManager::RxCharCB : public NimBLECharacteristicCallbacks
{
public:
    explicit RxCharCB(BLEManager *owner) : _owner(owner) {}

    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        (void)connInfo;
        std::string v = pCharacteristic->getValue();
        size_t n = v.size();
        if (n > BLEManager::RX_BUF_SIZE)
            n = BLEManager::RX_BUF_SIZE;

        memcpy(_owner->rxBuf, v.data(), n);
        _owner->rxLen = n;

        Serial.print("[BLE] RX write ");
        Serial.print(n);
        Serial.println(" bytes");
    }

private:
    BLEManager *_owner;
};

// -------------------- BLEManager --------------------

BLEManager::BLEManager() {}

uint16_t BLEManager::effectiveChunkSize() const
{
    uint16_t maxPayload = (_mtu > 23) ? (_mtu - 3) : 20;
    if (maxPayload > 180)
        maxPayload = 180;
    return maxPayload;
}

bool BLEManager::canSend() const
{
    return _connected && _notifySubscribed && (pTxChar != nullptr);
}

void BLEManager::startAdvertising()
{
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->stop();
    adv->start();
    Serial.println("[BLE] advertising restarted");
}

void BLEManager::begin()
{
    NimBLEDevice::init("ESP32-Telemetry");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    Serial.print("[BLE] MAC: ");
    Serial.println(NimBLEDevice::getAddress().toString().c_str());

    pServer = NimBLEDevice::createServer();

    static BLEManager::ServerCB serverCB(this);
    static BLEManager::TxCharCB txCB(this);
    static BLEManager::RxCharCB rxCB(this);

    pServer->setCallbacks(&serverCB);

    NimBLEService *service = pServer->createService(NimBLEUUID(SERVICE_UUID));

    // TX: notify/read
    pTxChar = service->createCharacteristic(
        NimBLEUUID(TX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pTxChar->setCallbacks(&txCB);
    pTxChar->addDescriptor(new NimBLE2904());

    // RX: write/write_no_resp
    pRxChar = service->createCharacteristic(
        NimBLEUUID(RX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pRxChar->setCallbacks(&rxCB);

    service->start();

    // Advertising: put service UUID in PRIMARY ADV so iOS filtered scan sees it
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->reset();

    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.addServiceUUID(NimBLEUUID(SERVICE_UUID));
    adv->setAdvertisementData(advData);

    NimBLEAdvertisementData scanResp;
    scanResp.setName("ESP32-Telemetry");
    adv->setScanResponseData(scanResp);

    adv->setMinInterval(48);
    adv->setMaxInterval(160);

    adv->start();
    Serial.println("[BLE] advertising started");
}

void BLEManager::sendBytes(const uint8_t *data, uint16_t len)
{
    if (!canSend() || !data || len == 0)
        return;

    uint16_t chunkSize = effectiveChunkSize();
    uint16_t offset = 0;

    while (offset < len)
    {
        if (!canSend())
            break;

        uint16_t chunk = (len - offset > chunkSize) ? chunkSize : (len - offset);

        pTxChar->setValue(data + offset, chunk);
        pTxChar->notify();

        offset += chunk;
        delay(2);
    }
}

void BLEManager::sendTelemetry(const TelemetryPacket &pkt)
{
    sendBytes(reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));
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
    rxLen = 0;
    return n;
}
