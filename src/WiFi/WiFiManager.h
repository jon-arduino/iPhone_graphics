#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <freertos/semphr.h>

#include "Protocol.h"
#include "BackChannelParser.h"

using DataCallback = std::function<void(const uint8_t *data, size_t len)>;

class WiFiManager
{
public:
    WiFiManager(const char *ssid, const char *password,
                const char *mdnsHostname = "esp32-uart",
                uint16_t tcpPort = 9000);

    void begin();

    // ── Configuration ─────────────────────────────────────────────────────────
    void setHeartbeat(uint32_t pingIntervalMs, uint32_t pongTimeoutMs)
    {
        _pingIntervalMs = pingIntervalMs;
        _pongTimeoutMs = pongTimeoutMs;
    }

    // ── Status ────────────────────────────────────────────────────────────────
    bool isConnected() const;
    bool clientConnected() const { return _client != nullptr; }
    size_t clientSpace() const { return _client ? _client->space() : 0; }

    // ── GFX data send (any task, serialised by _writeMutex) ───────────────────
    // Takes _writeMutex, writes all bytes, checks _pingNeeded before releasing.
    // A pending ping will be sent at the clean frame boundary before unlock.
    void send(const uint8_t *data, size_t len);
    void send(const char *str);

    // Framed back-channel command (used internally for ping; public for sendCmd)
    void sendCmd(uint8_t cmd, const uint8_t *payload = nullptr, size_t payloadLen = 0);

    // ── Heartbeat + maintenance (heartbeat task, ~100ms) ──────────────────────
    // Checks ping interval, sets _pingNeeded, tries to send ping.
    // Pong watchdog. Safe to call from any task.
    void update();

    // ── Callbacks ─────────────────────────────────────────────────────────────
    void onData(DataCallback cb) { _dataCallback = cb; }
    void onConnected(void (*cb)()) { _onConnected = cb; }
    void onDisconnected(void (*cb)()) { _onDisconnected = cb; }
    void onFirstPong(void (*cb)()) { _onFirstPong = cb; }
    // Forwarded to BackChannelParser:
    void onKey(void (*cb)(uint8_t key)) { _bc.onKey(cb); }
    void onTouch(void (*cb)(uint8_t cmd, int16_t x, int16_t y)) { _bc.onTouch(cb); }

private:
    const char *_ssid;
    const char *_password;
    const char *_mdnsHostname;
    uint16_t _tcpPort;

    AsyncServer *_server = nullptr;
    AsyncClient *_client = nullptr;

    // ── Write serialisation ───────────────────────────────────────────────────
    // Taken by any task writing to _client. Ensures GFX data and pings never
    // interleave — a ping always lands at a clean frame boundary.
    SemaphoreHandle_t _writeMutex = nullptr;

    // ── Heartbeat state ───────────────────────────────────────────────────────
    uint32_t _pingIntervalMs = 3000;
    uint32_t _pongTimeoutMs = 9000;
    uint32_t _lastPingSentMs = 0;
    bool _waitingForPong = false;
    bool _pingNeeded = false; // set when interval elapses, cleared when sent
    uint8_t _loggedThresholds = 0;

    // ── Callbacks ─────────────────────────────────────────────────────────────
    DataCallback _dataCallback;
    void (*_onConnected)() = nullptr;
    void (*_onDisconnected)() = nullptr;
    void (*_onFirstPong)() = nullptr;
    bool _firstPongReceived = false;

    // ── Back-channel parser ───────────────────────────────────────────────────
    BackChannelParser _bc;

    // ── Internals ─────────────────────────────────────────────────────────────
    void sendPingNow();
    void startMDNS();
    void startTCPServer();
    void onClientConnected(AsyncClient *client);
    void dropClient(const char *reason);
};