#include "BLEManager.h"
#include <NimBLE2904.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Callbacks
// ─────────────────────────────────────────────────────────────────────────────

void BLEManager::ServerCB::onConnect(NimBLEServer *, NimBLEConnInfo &connInfo)
{
    _owner->_connected = true;
    _owner->_notifySubscribed = false;
    _owner->_bc.reset();
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
    _owner->_bc.reset();
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
    xSemaphoreGive(_owner->_txDone);
}

void BLEManager::RxCharCB::onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &)
{
    std::string v = pChar->getValue();
    size_t n = v.size();
    _owner->_bc.feed(reinterpret_cast<const uint8_t *>(v.data()), n);
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
    // Use full MTU payload — sendBytes() blocks until each chunk is fully
    // ACK'd before sending the next, so only one mbuf is ever in flight.
    // No need to artificially cap — larger chunks = fewer round trips.
    uint16_t maxPayload = (_mtu > 23) ? (_mtu - 3) : 20;
    return maxPayload;
}

// ─────────────────────────────────────────────────────────────────────────────
//  sendBytes — semaphore-gated chunked notify
//
//  Splits data into MTU-sized chunks. For each chunk:
//    1. Take _txDone — blocks until onStatus() fires for previous chunk
//    2. Call notify()
//    3. onStatus() gives _txDone when stack has queued the packet
//
//  onStatus fires when NimBLE has accepted the packet into its queue —
//  not when the iPhone receives it (notifications have no application ACK).
//  This is still correct pacing: one chunk in the stack queue at a time,
//  which is all a NUS UART transport needs.
//
//  On return, _txDone is given (idle state) so the next sendBytes() call
//  can take it immediately without stalling.
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

        // Wait for previous chunk's onStatus. Timeout 2s as safety net.
        if (xSemaphoreTake(_txDone, pdMS_TO_TICKS(2000)) != pdTRUE)
        {
            Serial.println("[BLE] sendBytes timeout waiting for stack");
            xSemaphoreGive(_txDone);
            return;
        }

        if (!canSend())
        {
            xSemaphoreGive(_txDone);
            return;
        }

        _lastStatusCode = 0;
        pTxChar->setValue(data + off, n);

        bool ok = (_cccdIndicate) ? pTxChar->indicate() : pTxChar->notify();

        if (!ok)
        {
            // Rejected synchronously — restore and retry
            Serial.println("[BLE] notify() rejected synchronously");
            xSemaphoreGive(_txDone);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Wait for THIS chunk's onStatus before advancing
        if (xSemaphoreTake(_txDone, pdMS_TO_TICKS(2000)) != pdTRUE)
        {
            Serial.println("[BLE] sendBytes timeout waiting for onStatus");
            xSemaphoreGive(_txDone);
            return;
        }

        if (_lastStatusCode != 0)
        {
            // Transient error (BLE_HS_ENOMEM=6 most common) — back off and retry.
            // The stack recovers once the controller drains its queue.
            Serial.printf("[BLE] onStatus error=%d — retrying chunk\n", _lastStatusCode);
            vTaskDelay(pdMS_TO_TICKS(50));
            xSemaphoreGive(_txDone);
            continue;
        }

        xSemaphoreGive(_txDone); // restore idle state for next iteration
        off += n;                // chunk confirmed queued — advance
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