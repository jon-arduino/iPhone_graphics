#include "BLEManager.h"
#include <NimBLE2904.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Callbacks
// ─────────────────────────────────────────────────────────────────────────────

void BLEManager::ServerCB::onConnect(NimBLEServer *, NimBLEConnInfo &connInfo)
{
    _owner->_connected = true;
    _owner->_txClear();
    _owner->_bcLen = 0;
    _owner->_waitingForPong = false;
    _owner->_lastPingSentMs = millis();
    Serial.println("[BLE] connected");
    Serial.print("[BLE] peer: ");
    Serial.println(connInfo.getAddress().toString().c_str());
}

void BLEManager::ServerCB::onDisconnect(NimBLEServer *, NimBLEConnInfo &, int reason)
{
    _owner->_connected = false;
    _owner->_notifySubscribed = false;
    _owner->_cccdNotify = false;
    _owner->_cccdIndicate = false;
    _owner->_waitingForPong = false;
    _owner->_txClear();
    Serial.printf("[BLE] disconnected reason=%d\n", reason);
    _owner->startAdvertising();
}

void BLEManager::ServerCB::onMTUChange(uint16_t mtu, NimBLEConnInfo &)
{
    _owner->_mtu = mtu;
    Serial.printf("[BLE] MTU=%d\n", mtu);
}

void BLEManager::TxCharCB::onSubscribe(NimBLECharacteristic *,
                                       NimBLEConnInfo &,
                                       uint16_t subValue)
{
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

void BLEManager::TxCharCB::onStatus(NimBLECharacteristic *, int code)
{
    if (_owner->_txInFlight && _owner->_pendingLen > 0)
    {
        if (code == 0)
        {
            _owner->_txTail = (_owner->_txTail + _owner->_pendingLen) % BLEManager::TX_Q_SIZE;
        }
    }
    _owner->_pendingCode = code;
    _owner->_pendingLen = 0;
    _owner->_txInFlight = false;
}

void BLEManager::RxCharCB::onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &)
{
    std::string v = pChar->getValue();
    size_t n = v.size();

    // Route to back-channel parser first (handles pong frames)
    _owner->processBackChannel(
        reinterpret_cast<const uint8_t *>(v.data()), n);

    // Also copy to RX buffer for application use
    if (n > BLEManager::RX_BUF_SIZE)
        n = BLEManager::RX_BUF_SIZE;
    memcpy(_owner->rxBuf, v.data(), n);
    _owner->rxLen = n;
}

// ─────────────────────────────────────────────────────────────────────────────
//  BLEManager methods
// ─────────────────────────────────────────────────────────────────────────────

BLEManager::BLEManager() {}

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

    pTxChar = service->createCharacteristic(
        NimBLEUUID(TX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);
    pTxChar->setCallbacks(&txCB);
    pTxChar->addDescriptor(new NimBLE2904());

    pRxChar = service->createCharacteristic(
        NimBLEUUID(RX_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pRxChar->setCallbacks(&rxCB);

    service->start();

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

void BLEManager::startAdvertising()
{
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->stop();
    adv->start();
    Serial.println("[BLE] advertising restarted");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Heartbeat — call from loop()
//  Sends ping every pingIntervalMs, logs lateness thresholds.
//  Unlike WiFi we don't drop the connection on pong timeout — the BLE stack
//  handles disconnection. We log the problem and let the stack recover.
// ─────────────────────────────────────────────────────────────────────────────
void BLEManager::tick(uint32_t pingIntervalMs, uint32_t pongTimeoutMs)
{
    if (!canSend())
    {
        _waitingForPong = false;
        _lastPingSentMs = 0;
        _loggedThresholds = 0;
        return;
    }

    uint32_t now = millis();

    // Send ping on interval
    if (now - _lastPingSentMs >= pingIntervalMs)
    {
        sendPing();
        _lastPingSentMs = now;
        _waitingForPong = true;
        _loggedThresholds = 0;
    }

    // Log threshold crossings while waiting for pong
    if (_waitingForPong)
    {
        uint32_t elapsed = now - _lastPingSentMs;

        if (elapsed >= 500 && !(_loggedThresholds & 1))
        {
            _loggedThresholds |= 1;
            Serial.printf("[BLE] Pong late by %ums — loop may be slow\n", elapsed);
        }
        if (elapsed >= 1500 && !(_loggedThresholds & 2))
        {
            _loggedThresholds |= 2;
            Serial.printf("[BLE] Pong late by %ums — WARNING: significantly delayed\n", elapsed);
        }
        if (elapsed >= 6000 && !(_loggedThresholds & 4))
        {
            _loggedThresholds |= 4;
            Serial.printf("[BLE] Pong late by %ums — CRITICAL\n", elapsed);
        }

        // Log timeout but don't drop — BLE stack manages the connection
        if (elapsed >= pongTimeoutMs && !(_loggedThresholds & 8))
        {
            _loggedThresholds |= 8;
            Serial.printf("[BLE] Pong timeout (%ums) — BLE stack will disconnect if link is dead\n",
                          pongTimeoutMs);
        }
    }
}

void BLEManager::sendPing()
{
    if (!canSend())
        return;
    // Send directly via sendBytes — will be queued in TX ring and sent as BLE notification
    uint8_t frame[4] = {BLE_GFX_MAGIC, 0x01, 0x00, BLE_GFX_CMD_PING};
    sendBytes(frame, 4);
}

void BLEManager::pongReceived()
{
    _waitingForPong = false;
    _loggedThresholds = 0;
    Serial.println("[BLE] PONG received");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Back-channel parser (iPhone → ESP32 via RX characteristic)
//  Framing: [0xA5][lenLow][lenHigh][cmd][payload...]
//  Handles BLE_GFX_CMD_PONG (0xF1) internally.
// ─────────────────────────────────────────────────────────────────────────────
void BLEManager::processBackChannel(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        uint8_t b = data[i];

        if (_bcLen == 0)
        {
            if (b != BLE_GFX_MAGIC)
                continue;
        }

        if (_bcLen < sizeof(_bcBuf))
        {
            _bcBuf[_bcLen++] = b;
        }
        else
        {
            Serial.println("[BLE BackChannel] Buffer overrun — resync");
            _bcLen = 0;
            continue;
        }

        if (_bcLen < 3)
            continue;

        uint16_t frameLen = (uint16_t)_bcBuf[1] | ((uint16_t)_bcBuf[2] << 8);
        size_t totalSize = 3 + frameLen;

        if (frameLen < 1 || totalSize > sizeof(_bcBuf))
        {
            Serial.printf("[BLE BackChannel] Invalid len=%d — resync\n", frameLen);
            _bcLen = 0;
            continue;
        }

        if (_bcLen < totalSize)
            continue;

        uint8_t cmd = _bcBuf[3];
        _bcLen = 0;

        switch (cmd)
        {
        case BLE_GFX_CMD_PONG:
            pongReceived();
            break;
        default:
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  TX ring buffer
// ─────────────────────────────────────────────────────────────────────────────

uint16_t BLEManager::effectiveChunkSize() const
{
    uint16_t maxPayload = (_mtu > 23) ? (_mtu - 3) : 20;
    if (maxPayload > 120)
        maxPayload = 120;
    return maxPayload;
}

bool BLEManager::canSend() const
{
    return _connected && _notifySubscribed && (pTxChar != nullptr);
}

size_t BLEManager::_txCount() const
{
    size_t h = _txHead, t = _txTail;
    return (h >= t) ? (h - t) : (TX_Q_SIZE - t + h);
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
    if (!data || len == 0 || !canSend())
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
    if (!canSend() || _txInFlight)
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

void BLEManager::sendBytes(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || !canSend())
        return;
    size_t off = 0;
    while (off < len)
    {
        size_t pushed = _txEnqueueSome(data + off, len - off);
        off += pushed;
        _txDrain();
        if (_txInFlight)
            vTaskDelay(0);
        if (off < len)
            vTaskDelay(1);
    }
}

void BLEManager::pump_BLE_txQ() { _txDrain(); }

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

size_t BLEManager::txQueuedBytes() const { return _txCount(); }
size_t BLEManager::txFreeBytes() const { return _txSpace(); }

bool BLEManager::hasRxData() const { return rxLen > 0; }

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
