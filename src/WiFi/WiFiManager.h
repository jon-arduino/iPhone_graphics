#pragma once

#include <WiFi.h>   //not sure 
#include <ESPmDNS.h>
#include <AsyncTCP.h>

// Called when data arrives from iPhone
using DataCallback = std::function<void(const uint8_t* data, size_t len)>;

// Simple class for handling WiFi connections, mDNS advertisement, and AsyncTCP server
class WiFiManager
{
public:
    WiFiManager(const char* ssid, const char* password,
                const char* mdnsHostname = "esp32-uart",
                uint16_t    tcpPort      = 9000);

    void begin();
    bool isConnected() const;

    // Send bytes to the connected iPhone (if any)
    void send(const uint8_t* data, size_t len);
    void send(const char* str);

    // Register a callback for incoming data from iPhone
    void onData(DataCallback cb) { _dataCallback = cb; }

    // True if an iPhone client is currently connected
    bool clientConnected() const { return _client != nullptr; }

private:
    const char* _ssid;
    const char* _password;
    const char* _mdnsHostname;
    uint16_t    _tcpPort;

    AsyncServer*  _server  = nullptr;
    AsyncClient*  _client  = nullptr;   // only one client at a time

    DataCallback _dataCallback;

    void startMDNS();
    void startTCPServer();
    void onClientConnected(AsyncClient* client);
};