#include "WiFiManager.h"

WiFiManager::WiFiManager(const char* ssid, const char* password,
                         const char* mdnsHostname, uint16_t tcpPort)
    : _ssid(ssid)
    , _password(password)
    , _mdnsHostname(mdnsHostname)
    , _tcpPort(tcpPort)
{}

// ─────────────────────────────────────────────
//  Public
// ─────────────────────────────────────────────

void WiFiManager::begin()
{
    WiFi.begin(_ssid, _password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected — IP: %s\n", WiFi.localIP().toString().c_str());

    startMDNS();
    startTCPServer();
}

bool WiFiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

// Send bytes to iPhone with flow control.
// AsyncTCP's internal send buffer is finite (~5700 bytes by default).
// If we write faster than the iPhone drains it, write() silently drops
// data or the connection closes.  We check space() first and yield until
// there's room — this keeps the TCP pipeline full but never overflows it.
void WiFiManager::send(const uint8_t* data, size_t len)
{
    if (!_client || !_client->connected()) return;

    size_t sent = 0;
    const uint32_t timeoutMs = 2000;   // give up after 2 s of stalling
    uint32_t start = millis();

    while (sent < len) {
        // How many bytes can the send buffer accept right now?
        size_t available = _client->space();

        if (available == 0) {
            // Buffer full — yield to let AsyncTCP drain it, then retry
            if (millis() - start > timeoutMs) {
                Serial.printf("[WiFi] send timeout — dropped %d bytes\n",
                              (int)(len - sent));
                return;
            }
            delay(1);   // yield 1 ms — AsyncTCP drains on its task
            continue;
        }

        // Write as much as fits in one shot
        size_t chunk = min(available, len - sent);
        size_t written = _client->write(
            reinterpret_cast<const char*>(data + sent), chunk);

        if (written == 0) {
            // write() returned 0 — connection likely dropped
            Serial.println("[WiFi] write() returned 0, aborting send");
            return;
        }

        sent += written;
        start = millis();   // reset timeout after each successful write
    }
}

void WiFiManager::send(const char* str)
{
    send(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

// ─────────────────────────────────────────────
//  Private
// ─────────────────────────────────────────────

void WiFiManager::startMDNS()
{
    if (!MDNS.begin(_mdnsHostname)) {
        Serial.println("ERROR: mDNS failed to start");
        return;
    }

    MDNS.addService("uart", "tcp", _tcpPort);
    MDNS.addServiceTxt("uart", "tcp", "board",   "ESP32");
    MDNS.addServiceTxt("uart", "tcp", "version", "1.0");

    Serial.printf("mDNS advertising _uart._tcp as %s.local on port %d\n",
                  _mdnsHostname, _tcpPort);
}

void WiFiManager::startTCPServer()
{
    _server = new AsyncServer(_tcpPort);

    _server->onClient([](void* arg, AsyncClient* client) {
        static_cast<WiFiManager*>(arg)->onClientConnected(client);
    }, this);

    _server->begin();
    Serial.printf("TCP server listening on port %d\n", _tcpPort);
}

void WiFiManager::onClientConnected(AsyncClient* client)
{
    if (_client) {
        Serial.println("Replacing existing client");
        _client->close(true);
        _client = nullptr;
    }

    _client = client;
    Serial.printf("iPhone connected from %s\n", client->remoteIP().toString().c_str());

    // ── Data arriving from iPhone ─────────────────────────────────────────────
    client->onData([](void* arg, AsyncClient* c, void* data, size_t len) {
        auto* self = static_cast<WiFiManager*>(arg);

        // Respond to heartbeat ping — must pong or iPhone watchdog kills connection
        const char*  ping    = "__PING__";
        const size_t pingLen = 8;
        if (len >= pingLen && memcmp(data, ping, pingLen) == 0) {
            if (c->connected()) c->write("__PONG__\n", 9);
            return;
        }

        if (self->_dataCallback) {
            self->_dataCallback(static_cast<uint8_t*>(data), len);
        }
    }, this);

    // ── Client disconnected ───────────────────────────────────────────────────
    client->onDisconnect([](void* arg, AsyncClient* c) {
        auto* self = static_cast<WiFiManager*>(arg);
        Serial.println("iPhone disconnected");
        if (self->_client == c) self->_client = nullptr;
        delete c;
    }, this);

    // ── Network error ─────────────────────────────────────────────────────────
    client->onError([](void* arg, AsyncClient* c, int8_t error) {
        auto* self = static_cast<WiFiManager*>(arg);
        Serial.printf("Client error: %d\n", error);
        if (self->_client == c) self->_client = nullptr;
        delete c;
    }, this);

    // ── Timeout ───────────────────────────────────────────────────────────────
    client->onTimeout([](void* arg, AsyncClient* c, uint32_t time) {
        Serial.printf("Client timeout at %u ms\n", time);
        c->close();
    }, this);
}
