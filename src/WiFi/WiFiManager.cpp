#include "WiFiManager.h"

WiFiManager::WiFiManager(const char* ssid, const char* password,
                         const char* mdnsHostname, uint16_t tcpPort)
    : _ssid(ssid), _password(password)
    , _mdnsHostname(mdnsHostname), _tcpPort(tcpPort)
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

// Call from loop() every iteration.
// Sends a binary ping to the iPhone every PING_INTERVAL_MS.
// If no pong comes back within PONG_TIMEOUT_MS, the connection is dropped —
// AsyncTCP will fire onDisconnect which clears _client, and the next
// initWifiPhoneUI() edge detection in loop() will re-initialise when
// the iPhone reconnects.
void WiFiManager::tick()
{
    if (!_client || !_client->connected()) {
        _waitingForPong = false;
        _lastPingSentMs = 0;
        return;
    }

    uint32_t now = millis();

    // Send ping on interval
    if (now - _lastPingSentMs >= PING_INTERVAL_MS) {
        sendCmd(GFX_CMD_PING);
        _lastPingSentMs    = now;
        _waitingForPong    = true;
        _loggedThresholds  = 0;   // reset threshold flags for new ping episode
    }

    // While waiting for pong, log threshold crossings once each
    if (_waitingForPong) {
        uint32_t elapsed = now - _lastPingSentMs;

        // 500ms — loop may be slow
        if (elapsed >= 500 && !(_loggedThresholds & 1)) {
            _loggedThresholds |= 1;
            Serial.printf("[WiFi] Pong late by %ums — ESP32 loop may be slow\n", elapsed);
        }
        // 1500ms — significant delay
        if (elapsed >= 1500 && !(_loggedThresholds & 2)) {
            _loggedThresholds |= 2;
            Serial.printf("[WiFi] Pong late by %ums — WARNING: significantly delayed\n", elapsed);
        }
        // 6000ms — critical, about to drop
        if (elapsed >= 6000 && !(_loggedThresholds & 4)) {
            _loggedThresholds |= 4;
            Serial.printf("[WiFi] Pong late by %ums — CRITICAL: dropping connection\n", elapsed);
        }
    }

    // Watchdog — drop connection if pong not received within timeout
    if (_waitingForPong && (now - _lastPingSentMs >= PONG_TIMEOUT_MS)) {
        dropClient("pong timeout");
    }
}

bool WiFiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

// Send raw GFX bytes with flow control.
// Checks _client->space() to avoid overflowing AsyncTCP's send buffer
// under heavy graphics load (fillRect floods etc.)
void WiFiManager::send(const uint8_t* data, size_t len)
{
    if (!_client || !_client->connected()) return;

    size_t sent = 0;
    const uint32_t timeoutMs = 2000;
    uint32_t start = millis();

    while (sent < len) {
        size_t available = _client->space();

        if (available == 0) {
            if (millis() - start > timeoutMs) {
                Serial.printf("[WiFi] send timeout — dropped %d bytes\n", (int)(len - sent));
                return;
            }
            delay(1);
            continue;
        }

        size_t chunk   = min(available, len - sent);
        size_t written = _client->write(
            reinterpret_cast<const char*>(data + sent), chunk);

        if (written == 0) {
            Serial.println("[WiFi] write() returned 0, aborting");
            return;
        }

        sent  += written;
        start  = millis();
    }
}

void WiFiManager::send(const char* str)
{
    send(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

// Send a framed back-channel command: [0xA5][lenLow][lenHigh][cmd][payload...]
void WiFiManager::sendCmd(uint8_t cmd, const uint8_t* payload, size_t payloadLen)
{
    if (!_client || !_client->connected()) return;

    uint16_t len = 1 + (uint16_t)payloadLen;
    uint8_t hdr[4] = {
        GFX_MAGIC,
        (uint8_t)(len & 0xFF),
        (uint8_t)(len >> 8),
        cmd
    };

    send(hdr, 4);
    if (payloadLen > 0 && payload != nullptr) {
        send(payload, payloadLen);
    }
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
        _client->close();
        _client = nullptr;
    }

    _client         = client;
    _bcLen          = 0;       // reset back-channel parser
    _waitingForPong = false;   // reset heartbeat state
    _lastPingSentMs = millis(); // give iPhone time to settle before first ping

    Serial.printf("iPhone connected from %s\n",
                  client->remoteIP().toString().c_str());

    // ── Incoming data from iPhone ─────────────────────────────────────────────
    client->onData([](void* arg, AsyncClient* c, void* data, size_t len) {
        static_cast<WiFiManager*>(arg)
            ->processBackChannel(static_cast<uint8_t*>(data), len);
    }, this);

    // ── Disconnect ────────────────────────────────────────────────────────────
    client->onDisconnect([](void* arg, AsyncClient* c) {
        auto* self = static_cast<WiFiManager*>(arg);
        Serial.println("iPhone disconnected");
        if (self->_client == c) {
            self->_client       = nullptr;
            self->_waitingForPong = false;
        }
        delete c;
    }, this);

    // ── Error ─────────────────────────────────────────────────────────────────
    client->onError([](void* arg, AsyncClient* c, int8_t error) {
        auto* self = static_cast<WiFiManager*>(arg);
        Serial.printf("Client error: %d\n", error);
        if (self->_client == c) {
            self->_client       = nullptr;
            self->_waitingForPong = false;
        }
        delete c;
    }, this);

    // ── Timeout ───────────────────────────────────────────────────────────────
    client->onTimeout([](void* arg, AsyncClient* c, uint32_t time) {
        Serial.printf("Client TCP timeout at %u ms\n", time);
        c->close();
    }, this);
}

void WiFiManager::dropClient(const char* reason)
{
    Serial.printf("[WiFi] Dropping client: %s\n", reason);
    _waitingForPong = false;
    if (_client) {
        _client->close();
        _client = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Back-channel parser  (iPhone → ESP32)
//  Framing: [0xA5][lenLow][lenHigh][cmd][payload...]
//  GFX_CMD_PONG (0xF1) is handled internally — clears the watchdog.
//  Unknown commands are forwarded to _dataCallback (future UI events).
// ─────────────────────────────────────────────────────────────────────────────
void WiFiManager::processBackChannel(const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        if (_bcLen == 0) {
            if (b != GFX_MAGIC) continue;   // resync silently
        }

        if (_bcLen < sizeof(_bcBuf)) {
            _bcBuf[_bcLen++] = b;
        } else {
            Serial.println("[BackChannel] Buffer overrun — resync");
            _bcLen = 0;
            continue;
        }

        if (_bcLen < 3) continue;

        uint16_t frameLen  = (uint16_t)_bcBuf[1] | ((uint16_t)_bcBuf[2] << 8);
        size_t   totalSize = 3 + frameLen;

        if (frameLen < 1 || totalSize > sizeof(_bcBuf)) {
            Serial.printf("[BackChannel] Invalid len=%d — resync\n", frameLen);
            _bcLen = 0;
            continue;
        }

        if (_bcLen < totalSize) continue;   // wait for more bytes

        uint8_t        cmd        = _bcBuf[3];
        const uint8_t* payload    = (_bcLen > 4) ? &_bcBuf[4] : nullptr;
        size_t         payloadLen = (frameLen > 1) ? frameLen - 1 : 0;

        switch (cmd) {
            case GFX_CMD_PONG:
                // iPhone responded — connection confirmed alive
                _waitingForPong   = false;
                _loggedThresholds = 0;
                Serial.println("[WiFi] PONG received");
                break;

            default:
                // Future: button press, touch event, etc.
                if (_dataCallback && payloadLen > 0) {
                    _dataCallback(payload, payloadLen);
                }
                break;
        }

        _bcLen = 0;
    }
}
