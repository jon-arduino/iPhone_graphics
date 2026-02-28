#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <NimBLEDevice.h>
#include "telemetry/TelemetryPacket.h"

// Nordic UART Service-style UUIDs
static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *TX_CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *RX_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";

// Back-channel opcodes — shared with WiFiManager
// Framing: [0xA5][lenLow][lenHigh][cmd][payload...]
static constexpr uint8_t BLE_GFX_MAGIC = 0xA5;
static constexpr uint8_t BLE_GFX_CMD_PING = 0xF0; // ESP32 → iPhone
static constexpr uint8_t BLE_GFX_CMD_PONG = 0xF1; // iPhone → ESP32

class BLEManager
{
public:
    BLEManager();

    void begin();
    void startAdvertising();

    bool canSend() const;
    uint16_t effectiveChunkSize() const;

    void sendBytes(const uint8_t *data, uint16_t len);
    bool flushTx(uint32_t timeoutMs);
    void pump_BLE_txQ();

    // ── Heartbeat ──────────────────────────────────────────────────────────────
    // Call from loop() — sends ping every pingIntervalMs,
    // logs lateness thresholds if pong is delayed.
    // Unlike WiFi, BLE stack manages its own connection keepalive so we
    // don't drop the connection on pong timeout — we just log and let the
    // BLE stack handle disconnection naturally.
    void tick(uint32_t pingIntervalMs, uint32_t pongTimeoutMs);

    // Framed ping send: [0xA5][0x01][0x00][0xF0]
    void sendPing();

    // Call when iPhone sends a pong back over BLE RX characteristic
    void pongReceived();

    // Diagnostics
    size_t txQueuedBytes() const;
    size_t txFreeBytes() const;
    bool hasRxData() const;
    size_t readRx(uint8_t *dst, size_t maxLen);

private:
    friend class ServerCB;
    friend class TxCharCB;
    friend class RxCharCB;

    volatile bool _cccdNotify = false;
    volatile bool _cccdIndicate = false;
    volatile bool _txInFlight = false;
    volatile uint16_t _pendingLen = 0;
    volatile int _pendingCode = 0;

    NimBLEServer *pServer = nullptr;
    NimBLECharacteristic *pTxChar = nullptr;
    NimBLECharacteristic *pRxChar = nullptr;

    bool _connected = false;
    bool _notifySubscribed = false;
    uint16_t _mtu = 23;

    // ── Heartbeat state ───────────────────────────────────────────────────────
    uint32_t _lastPingSentMs = 0;
    bool _waitingForPong = false;
    uint8_t _loggedThresholds = 0; // bitmask: bit0=500ms bit1=1500ms bit2=6000ms

    // ── RX ────────────────────────────────────────────────────────────────────
    static constexpr size_t RX_BUF_SIZE = 256;
    uint8_t rxBuf[RX_BUF_SIZE];
    volatile size_t rxLen = 0;

    // ── TX ring buffer ────────────────────────────────────────────────────────
    static constexpr size_t TX_Q_SIZE = 8192;
    uint8_t _txQ[TX_Q_SIZE];
    volatile size_t _txHead = 0;
    volatile size_t _txTail = 0;

    uint32_t _lastNotifyMicros = 0;
    static constexpr uint32_t MIN_GAP_US = 500;
    static constexpr uint8_t MAX_NOTIFIES_PER_PUMP = 1;

    size_t _txCount() const;
    size_t _txSpace() const;
    void _txClear();
    size_t _txEnqueueSome(const uint8_t *data, size_t len);
    void _txDrain();

    // Back-channel RX parser state (iPhone → ESP32 pong frames)
    uint8_t _bcBuf[16];
    size_t _bcLen = 0;
    void processBackChannel(const uint8_t *data, size_t len);

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