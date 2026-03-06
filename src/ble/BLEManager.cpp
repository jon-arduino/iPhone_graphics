#include "BLEManager.h"
#include <NimBLE2904.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Callbacks
// ─────────────────────────────────────────────────────────────────────────────

void BLEManager::ServerCB::onConnect(NimBLEServer *, NimBLEConnInfo &connInfo)
{
    _owner->_connected = true;
    _owner->_notifySubscribed = false;
    _owner->_bcLen = 0;
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
    // Give semaphore so any sendBytes() blocked in the wait loop can exit
    xSemaphoreGive(_owner->_txDone);
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
        // Ensure semaphore starts available so first sendBytes() can proceed
        xSemaphoreGive(_owner->_txDone);
    }

    if (_owner->_subscribedCallback)
        _owner->_subscribedCallback(_owner->_notifySubscribed);
}

// Called by NimBLE stack when a notification/indication is acknowledged.
// Gives the semaphore so sendBytes() can send the next chunk.
void BLEManager::TxCharCB::onStatus(NimBLECharacteristic *, int code)
{
    _owner->_lastStatusCode = code;
    if (code != 0)
        Serial.printf("[BLE] onStatus error=%d\n", code);
    xSemaphoreGive(_owner->_txDone);
}

void BLEManager::RxCharCB::onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &)
{
    std::string v = pChar->getValue();
    size_t n = v.size();
    _owner->processBackChannel(reinterpret_cast<const uint8_t *>(v.data()), n);
    if (n > RX_BUF_SIZE)
        n = RX_BUF_SIZE;
    memcpy(_owner->rxBuf, v.data(), n);
    _owner->rxLen = n;
}

// ─────────────────────────────────────────────────────────────────────────────
//  BLEManager
// ─────────────────────────────────────────────────────────────────────────────

BLEManager::BLEManager()
{
    // Binary semaphore — starts taken. Given by onSubscribe (ready to send)
    // and onStatus (previous chunk acknowledged). Taken by sendBytes().
    _txDone = xSemaphoreCreateBinary();
    configASSERT(_txDone);
}

void BLEManager::begin()
{
    NimBLEDevice::init("ESP32-GPS");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    Serial.printf("[BLE] MAC: %s\n", NimBLEDevice::getAddress().toString().c_str());

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCB(this));

    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pTxChar = pService->createCharacteristic(
        TX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);
    pTxChar->setCallbacks(new TxCharCB(this));

    pRxChar = pService->createCharacteristic(
        RX_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pRxChar->setCallbacks(new RxCharCB(this));

    pService->start();
    startAdvertising();
}

void BLEManager::startAdvertising()
{
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);

    NimBLEAdvertisementData scanResp;
    scanResp.setName("ESP32-GPS");
    adv->setScanResponseData(scanResp);
    adv->setMinInterval(48);
    adv->setMaxInterval(160);
    adv->start();
    Serial.println("[BLE] advertising started");
}

bool BLEManager::canSend() const
{
    return _connected && _notifySubscribed && (pTxChar != nullptr);
}

uint16_t BLEManager::effectiveChunkSize() const
{
    uint16_t maxPayload = (_mtu > 23) ? (_mtu - 3) : 20;
    return (maxPayload > 180) ? 180 : maxPayload;
}

// ─────────────────────────────────────────────────────────────────────────────
//  sendBytes — the entire send path, no ring buffer
//
//  Splits data into MTU-sized chunks. For each chunk:
//    1. Take _txDone semaphore (blocks until previous chunk is ACKed, or
//       until connection drops which gives the semaphore from onDisconnect)
//    2. Call notify/indicate
//    3. onStatus() gives _txDone when ACK arrives → next chunk proceeds
//
//  This is exactly the flow BLE is designed for. The stack paces us naturally.
// ─────────────────────────────────────────────────────────────────────────────
void BLEManager::sendBytes(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0)
        return;

    uint16_t chunk = effectiveChunkSize();
    uint16_t off = 0;

    while (off < len)
    {
        if (!canSend())
            return;

        uint16_t n = min(chunk, (uint16_t)(len - off));

        // Wait for previous ACK (or disconnect). Timeout 2s as safety net.
        if (xSemaphoreTake(_txDone, pdMS_TO_TICKS(2000)) != pdTRUE)
        {
            Serial.println("[BLE] sendBytes timeout waiting for ACK");
            return;
        }

        if (!canSend())
            return;

        _lastStatusCode = 0; // clear before each attempt
        pTxChar->setValue(data + off, n);

        bool ok = false;
        if (_cccdIndicate)
            ok = pTxChar->indicate();
        else if (_cccdNotify)
            ok = pTxChar->notify();

        if (!ok)
        {
            // notify() rejected before reaching stack — give semaphore back and retry
            xSemaphoreGive(_txDone);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // onStatus() sets _lastStatusCode and gives _txDone when ACK/error arrives.
        // On error (BLE_HS_ENOMEM=6 etc.) back off and retry the same chunk.
        if (_lastStatusCode != 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue; // don't advance off — resend same chunk
        }

        off += n; // success
    }
}

bool BLEManager::hasRxData() const { return rxLen > 0; }

size_t BLEManager::readRx(uint8_t *dst, size_t maxLen)
{
    size_t n = (rxLen < maxLen) ? rxLen : maxLen;
    memcpy(dst, rxBuf, n);
    rxLen = 0;
    return n;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Back-channel parser (iPhone → ESP32)
// ─────────────────────────────────────────────────────────────────────────────
void BLEManager::processBackChannel(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        if (_bcLen == 0)
        {
            if (b != BC_MAGIC)
                continue;
        }
        if (_bcLen < sizeof(_bcBuf))
        {
            _bcBuf[_bcLen++] = b;
        }
        else
        {
            _bcLen = 0;
            continue;
        }
        if (_bcLen < 3)
            continue;

        uint16_t frameLen = (uint16_t)_bcBuf[1] | ((uint16_t)_bcBuf[2] << 8);
        size_t totalSize = 3 + frameLen;
        if (frameLen < 1 || totalSize > sizeof(_bcBuf))
        {
            _bcLen = 0;
            continue;
        }
        if (_bcLen < totalSize)
            continue;

        uint8_t cmd = _bcBuf[3];
        switch (cmd)
        {
        case BC_CMD_KEY1:
            Serial.println("[BackChannel] KEY1");
            if (_keyCallback)
                _keyCallback('1');
            break;
        case BC_CMD_KEY2:
            Serial.println("[BackChannel] KEY2");
            if (_keyCallback)
                _keyCallback('2');
            break;
        default:
            break;
        }
        _bcLen = 0;
    }
}