#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>

using DataCallback = std::function<void(const uint8_t* data, size_t len)>;

// Back-channel protocol opcodes
// Framing: [0xA5][lenLow][lenHigh][cmd][payload...]
static constexpr uint8_t GFX_MAGIC    = 0xA5;
static constexpr uint8_t GFX_CMD_PING = 0xF0;   // ESP32 → iPhone (we send)
static constexpr uint8_t GFX_CMD_PONG = 0xF1;   // iPhone → ESP32 (we receive)

// Heartbeat timing
static constexpr uint32_t PING_INTERVAL_MS = 3000;   // send ping every 3s
static constexpr uint32_t PONG_TIMEOUT_MS  = 9000;   // drop connection if no pong in 9s

class WiFiManager
{
public:
    WiFiManager(const char* ssid, const char* password,
                const char* mdnsHostname = "esp32-uart",
                uint16_t    tcpPort      = 9000);

    void begin();

    // Call from loop() — sends periodic pings, checks pong watchdog
    void tick();

    bool isConnected()    const;
    bool clientConnected() const { return _client != nullptr; }

    // Send raw GFX bytes to iPhone (graphics pipeline, flow-controlled)
    void send(const uint8_t* data, size_t len);
    void send(const char* str);

    // Send a framed back-channel command: [0xA5][lenLow][lenHigh][cmd][payload...]
    void sendCmd(uint8_t cmd, const uint8_t* payload = nullptr, size_t payloadLen = 0);

    void onData(DataCallback cb) { _dataCallback = cb; }

private:
    const char* _ssid;
    const char* _password;
    const char* _mdnsHostname;
    uint16_t    _tcpPort;

    AsyncServer* _server = nullptr;
    AsyncClient* _client = nullptr;

    DataCallback _dataCallback;

    // Heartbeat state
    uint32_t _lastPingSentMs    = 0;
    bool     _waitingForPong    = false;
    uint8_t  _loggedThresholds  = 0;   // bitmask: bit0=500ms, bit1=1500ms, bit2=6000ms

    // Back-channel parser state (iPhone → ESP32 framed messages)
    uint8_t  _bcBuf[64];
    size_t   _bcLen = 0;

    void startMDNS();
    void startTCPServer();
    void onClientConnected(AsyncClient* client);
    void dropClient(const char* reason);

    // Parse incoming back-channel bytes; handles pong internally
    void processBackChannel(const uint8_t* data, size_t len);
};
