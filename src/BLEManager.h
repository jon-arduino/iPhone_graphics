#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <NimBLEDevice.h>
#include "TelemetryPacket.h"

// Nordic UART Service-style UUIDs
static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *TX_CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // ESP32 -> iPhone (Notify)
static const char *RX_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // iPhone -> ESP32 (Write)


// Your existing UUIDs, RX_BUF_SIZE, etc...
// #define SERVICE_UUID ...
// #define TX_CHARACTERISTIC_UUID ...
// #define RX_CHARACTERISTIC_UUID ...
// static constexpr size_t RX_BUF_SIZE = ...

class BLEManager
{
public:
    BLEManager();

    void begin();
    void startAdvertising();

    bool canSend() const;
    uint16_t effectiveChunkSize() const;

    // Existing API (now becomes "enqueue")
    void sendBytes(const uint8_t *data, uint16_t len);

    //Flush any queued TX data within the given timeout. 
    // Returns true if all data was sent, false if timeout hit with data still queued.
    bool flushTx(uint32_t timeoutMs);

    // NEW: call frequently from loop() to actually notify()
    void pump();

   // Public diagnostics (do not expose ring internals)
    size_t txQueuedBytes() const;
    size_t txFreeBytes() const;

    // Existing RX API
    bool hasRxData() const;
    size_t readRx(uint8_t *dst, size_t maxLen);

private:
    friend class ServerCB;
    friend class TxCharCB;
    friend class RxCharCB;

    // CCCD state (what the client enabled)
    volatile bool _cccdNotify = false;
    volatile bool _cccdIndicate = false;

    // TX in-flight state (commit ring pop on onStatus)
    volatile bool _txInFlight = false;
    volatile uint16_t _pendingLen = 0;
    volatile int _pendingCode = 0;

    // Used to request draining from callback context (avoid re-entrancy)
    volatile bool _kickDrain = false;

    // --- NimBLE ---
    NimBLEServer *pServer = nullptr;
    NimBLECharacteristic *pTxChar = nullptr;
    NimBLECharacteristic *pRxChar = nullptr;

    bool _connected = false;
    bool _notifySubscribed = false;
    uint16_t _mtu = 23;

    // --- RX ---
    static constexpr size_t RX_BUF_SIZE = 256;
    uint8_t rxBuf[RX_BUF_SIZE];
    volatile size_t rxLen = 0;

    // --- TX ring buffer ---
    // Make this big enough for your worst burst. 8K is a good start.
    static constexpr size_t TX_Q_SIZE = 8192;
    uint8_t _txQ[TX_Q_SIZE];
    volatile size_t _txHead = 0; // write index
    volatile size_t _txTail = 0; // read index
 

    // pacing (prevents iOS notification backlog)
    uint32_t _lastNotifyMicros = 0;
    static constexpr uint32_t MIN_GAP_US = 500; // ~.5ms between notifies
    static constexpr uint8_t MAX_NOTIFIES_PER_PUMP = 1;

    // helpers
    size_t _txCount() const;
    size_t _txSpace() const;
    void _txClear();

    // move bytes into ring buffer
    size_t _txEnqueueSome(const uint8_t *data, size_t len);

    // drain ring buffer via notify
    void _txDrain();

    // callbacks
    class ServerCB : public NimBLEServerCallbacks
    {
    public:
        explicit ServerCB(BLEManager *owner) : _owner(owner) {}
        void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;
        void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;
        void onMTUChange(uint16_t mtu, NimBLEConnInfo &connInfo) override;

    private:
        BLEManager *_owner;
    };
    class TxCharCB : public NimBLECharacteristicCallbacks
    {
    public:
        explicit TxCharCB(BLEManager *owner) : _owner(owner) {}

        void onSubscribe(NimBLECharacteristic *pCharacteristic,
                         NimBLEConnInfo &connInfo,
                         uint16_t subValue) override;

        void onStatus(NimBLECharacteristic *pCharacteristic, int code) override;

    private:
        BLEManager *_owner;
    };

    class RxCharCB : public NimBLECharacteristicCallbacks
    {
    public:
        explicit RxCharCB(BLEManager *owner) : _owner(owner) {}
        void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;

    private:
        BLEManager *_owner;
    };
};
