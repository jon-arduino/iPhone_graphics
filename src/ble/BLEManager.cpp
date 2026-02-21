#include "BLEManager.h"
#include <NimBLE2904.h>

// -------------------- BLEManager --------------------

BLEManager::BLEManager() {}

// -------------------- Callback Implementations --------------------

void BLEManager::ServerCB::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
{
    (void)pServer;
    _owner->_connected = true;
    _owner->_txClear(); // start clean on new connection
    Serial.println("[BLE] connected");
    Serial.print("[BLE] peer: ");
    Serial.println(connInfo.getAddress().toString().c_str());
}

void BLEManager::ServerCB::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
{
    (void)pServer;
    (void)connInfo;
    _owner->_connected = false;
    _owner->_notifySubscribed = false;
    _owner->_cccdNotify = false;
    _owner->_cccdIndicate = false;

    _owner->_txClear(); // IMPORTANT: drop queued bytes on disconnect
    Serial.print("[BLE] disconnected reason=");
    Serial.println(reason);
    _owner->startAdvertising();
}

void BLEManager::ServerCB::onMTUChange(uint16_t mtu, NimBLEConnInfo &connInfo)
{
    (void)connInfo;
    _owner->_mtu = mtu;
    Serial.print("[BLE] MTU=");
    Serial.println(mtu);
}

void BLEManager::TxCharCB::onSubscribe(NimBLECharacteristic *pCharacteristic,
                                       NimBLEConnInfo &connInfo,
                                       uint16_t subValue)
{
    (void)pCharacteristic;
    (void)connInfo;

    // CCCD bits:
    // 0x0001 = notifications enabled
    // 0x0002 = indications enabled
    _owner->_cccdNotify = (subValue & 0x0001) != 0;
    _owner->_cccdIndicate = (subValue & 0x0002) != 0;

    _owner->_notifySubscribed = _owner->_cccdNotify || _owner->_cccdIndicate;

    Serial.printf("[BLE] subscribed notify=%s indicate=%s\n",
                  _owner->_cccdNotify ? "YES" : "NO",
                  _owner->_cccdIndicate ? "YES" : "NO");

    if (_owner->_notifySubscribed)
    {
        _owner->_lastNotifyMicros = 0;
        _owner->_txInFlight = false;
        _owner->_pendingLen = 0;
    }
}

void BLEManager::TxCharCB::onStatus(NimBLECharacteristic *pCharacteristic, int code)
{
    (void)pCharacteristic;

    static uint32_t okCount = 0;
    static uint32_t failCount = 0;
    static bool debug_txStatus = false;

    if (code == 0)
        okCount++;
    else
        failCount++;

    // Commit or retry the in-flight chunk.
    if (_owner->_txInFlight && _owner->_pendingLen > 0)
    {
        if (code == 0)
        {
            _owner->_txTail = (_owner->_txTail + _owner->_pendingLen) % BLEManager::TX_Q_SIZE;
        }
        // else: leave _txTail unchanged so the same bytes will be resent
    }

    _owner->_pendingCode = code;
    _owner->_pendingLen = 0;
    _owner->_txInFlight = false;

    // Throttled debug print
    uint32_t total = okCount + failCount;
    if (((total % 100) == 0) && debug_txStatus)
    {
        Serial.printf("[BLE] txStatus ok=%lu fail=%lu queued=%u inflight=%d lastCode=%d\n",
                      (unsigned long)okCount,
                      (unsigned long)failCount,
                      (unsigned)_owner->_txCount(),
                      (int)_owner->_txInFlight,
                      code);
    }
}

void BLEManager::RxCharCB::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
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

// -------------------- BLEManager methods --------------------

uint16_t BLEManager::effectiveChunkSize() const
{
    // ATT payload = MTU - 3 (opcode+handle)
    uint16_t maxPayload = (_mtu > 23) ? (_mtu - 3) : 20;
    if (maxPayload > 120)
        maxPayload = 120;
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

    // TX: read + notify + indicate (support both; client may enable either)
    pTxChar = service->createCharacteristic(
        NimBLEUUID(TX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::INDICATE);

    pTxChar->setCallbacks(&txCB);
    pTxChar->addDescriptor(new NimBLE2904());

    // RX: write/write_no_resp
    pRxChar = service->createCharacteristic(
        NimBLEUUID(RX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pRxChar->setCallbacks(&rxCB);

    service->start();

    // Advertising
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

    _txClear();
}

// -------- TX ring buffer helpers --------

size_t BLEManager::_txCount() const
{
    size_t h = _txHead;
    size_t t = _txTail;
    if (h >= t)
        return h - t;
    return (TX_Q_SIZE - t) + h;
}

size_t BLEManager::_txSpace() const
{
    return (TX_Q_SIZE - 1) - _txCount();
}

void BLEManager::_txClear()
{
    _txHead = 0;
    _txTail = 0;
    _txInFlight = false;
    _pendingLen = 0;
    _pendingCode = 0;
}

size_t BLEManager::_txEnqueueSome(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return 0;
    if (!canSend())
        return 0;

    size_t space = _txSpace();
    if (space == 0)
        return 0;

    size_t n = (len > space) ? space : len;

    size_t h = _txHead;
    size_t first = min(n, TX_Q_SIZE - h);
    memcpy(&_txQ[h], data, first);
    if (n > first)
        memcpy(&_txQ[0], data + first, n - first);

    _txHead = (h + n) % TX_Q_SIZE;
    return n;
}

void BLEManager::_txDrain()
{
    if (!canSend())
        return;
    if (_txInFlight)
        return;

    size_t available = _txCount();
    if (available == 0)
        return;

    uint32_t now = micros();
    if (_lastNotifyMicros != 0 && (now - _lastNotifyMicros) < MIN_GAP_US)
        return;

    uint8_t tmp[180];
    uint16_t chunkMax = min<uint16_t>(effectiveChunkSize(), sizeof(tmp));
    uint16_t chunk = (available > chunkMax) ? chunkMax : (uint16_t)available;

    size_t t = _txTail;
    size_t first = min((size_t)chunk, TX_Q_SIZE - t);
    memcpy(tmp, &_txQ[t], first);
    if ((size_t)chunk > first)
        memcpy(tmp + first, &_txQ[0], chunk - first);

    pTxChar->setValue(tmp, chunk);

    bool ok = false;
    if (_cccdIndicate)
        ok = pTxChar->indicate();
    else if (_cccdNotify)
        ok = pTxChar->notify();
    else
        return;

    if (!ok)
        return;

    _pendingLen = chunk;
    _txInFlight = true;
    _lastNotifyMicros = now;
}

// -------- Public TX API --------

void BLEManager::sendBytes(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0)
        return;
    if (!canSend())
        return;

    size_t off = 0;
    while (off < len)
    {
        size_t pushed = _txEnqueueSome(data + off, len - off);
        off += pushed;

        _txDrain();

        // Yield if we're blocked by in-flight TX (keeps BLE host running)
        if (_txInFlight)
            vTaskDelay(0);

        if (off < len)
            vTaskDelay(1);
    }
}

void BLEManager::pump_BLE_txQ()
{
    _txDrain();
}

bool BLEManager::flushTx(uint32_t timeoutMs)
{
    uint32_t start = millis();
    while (millis() - start < timeoutMs)
    {
        _txDrain();
        if (_txCount() == 0 && !_txInFlight)
            return true;
        vTaskDelay(1);
    }
    return false;
}

// Public diagnostics (do not expose ring internals)
size_t BLEManager::txQueuedBytes() const { return _txCount(); }
size_t BLEManager::txFreeBytes() const { return _txSpace(); }


// -------- RX API --------

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
