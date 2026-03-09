#pragma once

#include <Arduino.h>
#include <functional>
#include <NimBLEDevice.h>
#include <freertos/semphr.h>
#include "Protocol.h"
#include "BackChannelParser.h"

// Nordic UART Service UUIDs
static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *TX_CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *RX_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";

class BLEManager
{
public:
    BLEManager();

    void begin();
    void startAdvertising();

    bool canSend() const;
    uint16_t effectiveChunkSize() const;

    // Send bytes — blocks until all bytes are delivered (one MTU-chunk at a time).
    // Each chunk waits for BLE stack acknowledgment before sending the next.
    // No ring buffer, no background drain — the BLE stack drives pacing naturally.
    void sendBytes(const uint8_t *data, uint16_t len);

    // No-op — heartbeat task not needed without ring buffer.
    // Kept so call sites in main.cpp compile without change.
    void update() {}

    // Callbacks — forwarded to BackChannelParser or handled locally
    void onKey(std::function<void(uint8_t)> cb) { _bc.onKey(cb); }
    void onTouch(std::function<void(uint8_t, int16_t, int16_t)> cb) { _bc.onTouch(cb); }
    void onSubscribed(void (*cb)(bool ready)) { _subscribedCallback = cb; }

    // RX
    bool hasRxData() const;
    size_t readRx(uint8_t *dst, size_t maxLen);

private:
    friend class ServerCB;
    friend class TxCharCB;
    friend class RxCharCB;

    NimBLEServer *pServer = nullptr;
    NimBLECharacteristic *pTxChar = nullptr;
    NimBLECharacteristic *pRxChar = nullptr;

    bool _connected = false;
    bool _notifySubscribed = false;
    uint16_t _mtu = 23;

    volatile bool _cccdNotify = false;
    volatile bool _cccdIndicate = false;
    volatile int _lastStatusCode = 0;

    // Semaphore: taken before notify(), given by onStatus() when ACK arrives.
    // sendBytes() blocks here — BLE stack naturally paces transmission.
    SemaphoreHandle_t _txDone = nullptr;

    // RX
    static constexpr size_t RX_BUF_SIZE = 256;
    uint8_t rxBuf[RX_BUF_SIZE];
    volatile size_t rxLen = 0;

    // Back-channel parser — framing and dispatch shared with WiFiManager
    BackChannelParser _bc;

    void (*_subscribedCallback)(bool ready) = nullptr;

    class ServerCB : public NimBLEServerCallbacks
    {
    public:
        explicit ServerCB(BLEManager *o) : _owner(o) {}
        void onConnect(NimBLEServer *, NimBLEConnInfo &) override;
        void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override;
        void onMTUChange(uint16_t mtu, NimBLEConnInfo &) override;

    private:
        BLEManager *_owner;
    };

    class TxCharCB : public NimBLECharacteristicCallbacks
    {
    public:
        explicit TxCharCB(BLEManager *o) : _owner(o) {}
        void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &, uint16_t) override;
        void onStatus(NimBLECharacteristic *, int) override;

    private:
        BLEManager *_owner;
    };

    class RxCharCB : public NimBLECharacteristicCallbacks
    {
    public:
        explicit RxCharCB(BLEManager *o) : _owner(o) {}
        void onWrite(NimBLECharacteristic *, NimBLEConnInfo &) override;

    private:
        BLEManager *_owner;
    };
};