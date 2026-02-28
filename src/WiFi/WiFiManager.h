#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>

using DataCallback = std::function<void(const uint8_t *data, size_t len)>;

// Back-channel protocol opcodes
// Framing: [0xA5][lenLow][lenHigh][cmd][payload...]
static constexpr uint8_t GFX_MAGIC = 0xA5;
static constexpr uint8_t GFX_CMD_PING = 0xF0; // ESP32 → iPhone (we send)
static constexpr uint8_t GFX_CMD_PONG = 0xF1; // iPhone → ESP32 (we receive)

// Heartbeat timing
// Heartbeat timing constants are set in main.cpp and passed to tick()

class WiFiManager
{
public:
    WiFiManager(const char *ssid, const char *password,
                const char *mdnsHostname = "esp32-uart",
                uint16_t tcpPort = 9000);

    void begin();

    // Call from loop() — sends periodic pings, checks pong watchdog
    void tick(uint32_t pingIntervalMs, uint32_t pongTimeoutMs);

    bool isConnected() const;
    bool clientConnected() const { return _client != nullptr; }
    size_t clientSpace() const { return _client ? _client->space() : 0; }

    // Send raw GFX bytes to iPhone (graphics pipeline, flow-controlled)
    void send(const uint8_t *data, size_t len);
    void send(const char *str);

    // Send a framed back-channel command: [0xA5][lenLow][lenHigh][cmd][payload...]
    void sendCmd(uint8_t cmd, const uint8_t *payload = nullptr, size_t payloadLen = 0);

    void onData(DataCallback cb) { _dataCallback = cb; }
    void onDisconnected(void (*cb)()) { _onDisconnected = cb; }
    void onFirstPong(void (*cb)()) { _onFirstPong = cb; }

private:
    const char *_ssid;
    const char *_password;
    const char *_mdnsHostname;
    uint16_t _tcpPort;

    AsyncServer *_server = nullptr;
    AsyncClient *_client = nullptr;

    DataCallback _dataCallback;
    void (*_onDisconnected)() = nullptr;
    void (*_onFirstPong)() = nullptr;
    bool _firstPongReceived = false;

    // Heartbeat state
    uint32_t _lastPingSentMs = 0;
    bool _waitingForPong = false;
    uint8_t _loggedThresholds = 0; // bitmask: bit0=500ms, bit1=1500ms, bit2=6000ms

    // Back-channel parser state (iPhone → ESP32 framed messages)
    uint8_t _bcBuf[64];
    size_t _bcLen = 0;

    void startMDNS();
    void startTCPServer();
    void onClientConnected(AsyncClient *client);
    void dropClient(const char *reason);

    // Parse incoming back-channel bytes; handles pong internally
    void processBackChannel(const uint8_t *data, size_t len);
};