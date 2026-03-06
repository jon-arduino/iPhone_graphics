#include "WiFiManager.h"

WiFiManager::WiFiManager(const char *ssid, const char *password,
                         const char *mdnsHostname, uint16_t tcpPort)
    : _ssid(ssid), _password(password), _mdnsHostname(mdnsHostname), _tcpPort(tcpPort)
{
    _writeMutex = xSemaphoreCreateMutex();
    configASSERT(_writeMutex);
}

void WiFiManager::begin()
{
    WiFi.begin(_ssid, _password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected — IP: %s\n", WiFi.localIP().toString().c_str());
    startMDNS();
    startTCPServer();
}

// ─────────────────────────────────────────────────────────────────────────────
//  update() — heartbeat task, every ~100ms
//
//  1. Pong lateness logging and watchdog.
//  2. Set _pingNeeded if interval has elapsed.
//  3. Try to take _writeMutex with zero timeout:
//       got it  → send ping now, release.
//       didn't  → a GFX write is in progress; _pingNeeded stays set.
//                 send() will call sendPingNow() before it releases the mutex.
// ─────────────────────────────────────────────────────────────────────────────
void WiFiManager::update()
{
    if (!_client || !_client->connected())
    {
        _waitingForPong = false;
        _lastPingSentMs = 0;
        _pingNeeded = false;
        return;
    }

    uint32_t now = millis();

    // Pong watchdog logging
    if (_waitingForPong)
    {
        uint32_t el = now - _lastPingSentMs;
        if (el >= 500 && !(_loggedThresholds & 1))
        {
            _loggedThresholds |= 1;
            Serial.printf("[WiFi] Pong late by %ums — loop may be slow\n", el);
        }
        if (el >= 1500 && !(_loggedThresholds & 2))
        {
            _loggedThresholds |= 2;
            Serial.printf("[WiFi] Pong late by %ums — WARNING: significantly delayed\n", el);
        }
        if (el >= 6000 && !(_loggedThresholds & 4))
        {
            _loggedThresholds |= 4;
            Serial.printf("[WiFi] Pong late by %ums — CRITICAL: dropping\n", el);
        }
        if (_waitingForPong && (now - _lastPingSentMs >= _pongTimeoutMs))
        {
            dropClient("pong timeout");
            return;
        }
    }

    // Mark ping needed if interval elapsed
    if (now - _lastPingSentMs >= _pingIntervalMs)
        _pingNeeded = true;

    if (!_pingNeeded)
        return;

    // Try to get write token — zero timeout so we never block the heartbeat task.
    // If a GFX write holds the mutex, we leave _pingNeeded set and send() will
    // fire the ping at the frame boundary before it releases the mutex.
    if (xSemaphoreTake(_writeMutex, 0) == pdTRUE)
    {
        sendPingNow();
        xSemaphoreGive(_writeMutex);
    }
    // else: _pingNeeded stays true — send() will catch it
}

// ─────────────────────────────────────────────────────────────────────────────
//  send() — GFX data write, any task
//  Takes _writeMutex for the full duration of the write.
//  Checks _pingNeeded on exit — sends ping at the clean frame boundary
//  before releasing, so heartbeat task sees the ping go out before unlock.
// ─────────────────────────────────────────────────────────────────────────────
void WiFiManager::send(const uint8_t *data, size_t len)
{
    if (!_client || !_client->connected())
        return;

    xSemaphoreTake(_writeMutex, portMAX_DELAY);

    size_t sent = 0;
    const uint32_t timeoutMs = 2000;
    uint32_t start = millis();

    while (sent < len)
    {
        size_t available = _client->space();
        if (available == 0)
        {
            if (millis() - start > timeoutMs)
            {
                Serial.printf("[WiFi] send timeout — dropped %d bytes\n", (int)(len - sent));
                break;
            }
            delay(1);
            continue;
        }
        size_t chunk = min(available, len - sent);
        size_t written = _client->write(reinterpret_cast<const char *>(data + sent), chunk);
        if (written == 0)
        {
            Serial.println("[WiFi] write() returned 0, aborting");
            break;
        }
        sent += written;
        start = millis();
    }

    // Send any deferred ping at this clean frame boundary before unlocking
    if (_pingNeeded)
        sendPingNow();

    xSemaphoreGive(_writeMutex);
}

void WiFiManager::send(const char *str)
{
    send(reinterpret_cast<const uint8_t *>(str), strlen(str));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sendPingNow() — caller MUST hold _writeMutex
// ─────────────────────────────────────────────────────────────────────────────
void WiFiManager::sendPingNow()
{
    if (!_client || !_client->connected())
    {
        _pingNeeded = false;
        return;
    }
    uint8_t frame[4] = {BC_MAGIC,
                        0x01, 0x00, // length = 1 (just cmd byte)
                        GFX_CMD_PING};
    _client->write(reinterpret_cast<const char *>(frame), 4);
    _lastPingSentMs = millis();
    _waitingForPong = true;
    _loggedThresholds = 0;
    _pingNeeded = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  sendCmd() — framed back-channel command, takes mutex
// ─────────────────────────────────────────────────────────────────────────────
void WiFiManager::sendCmd(uint8_t cmd, const uint8_t *payload, size_t payloadLen)
{
    uint16_t len = 1 + (uint16_t)payloadLen;
    uint8_t hdr[4] = {BC_MAGIC, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8), cmd};
    send(hdr, 4);
    if (payloadLen > 0 && payload)
        send(payload, payloadLen);
}

bool WiFiManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }

// ─────────────────────────────────────────────────────────────────────────────
//  Private — unchanged from previous version
// ─────────────────────────────────────────────────────────────────────────────

void WiFiManager::startMDNS()
{
    if (!MDNS.begin(_mdnsHostname))
    {
        Serial.println("ERROR: mDNS failed");
        return;
    }
    MDNS.addService("uart", "tcp", _tcpPort);
    MDNS.addServiceTxt("uart", "tcp", "board", "ESP32");
    MDNS.addServiceTxt("uart", "tcp", "version", "1.0");
    Serial.printf("mDNS advertising _uart._tcp as %s.local on port %d\n", _mdnsHostname, _tcpPort);
}

void WiFiManager::startTCPServer()
{
    _server = new AsyncServer(_tcpPort);
    _server->onClient([](void *arg, AsyncClient *client)
                      { static_cast<WiFiManager *>(arg)->onClientConnected(client); }, this);
    _server->begin();
    Serial.printf("TCP server listening on port %d\n", _tcpPort);
}

void WiFiManager::onClientConnected(AsyncClient *client)
{
    bool wasConnected = (_client != nullptr);

    if (_client)
    {
        // Null _client BEFORE calling close() so the old client's async
        // onDisconnect fires later but the guard (self->_client == c) rejects it.
        AsyncClient *prev = _client;
        _client = nullptr;
        prev->close();
    }

    // Only fire onConnected for reconnects (previous client existed).
    // First connection goes through the normal changed-transport path in loop().
    if (wasConnected && _onConnected)
        _onConnected();

    _client = client;
    _bcLen = 0;
    _waitingForPong = false;
    _pingNeeded = false;
    _lastPingSentMs = millis();
    _firstPongReceived = false;

    Serial.printf("iPhone connected from %s\n", client->remoteIP().toString().c_str());

    client->onData([](void *arg, AsyncClient *c, void *data, size_t len)
                   { static_cast<WiFiManager *>(arg)->processBackChannel(static_cast<uint8_t *>(data), len); }, this);

    client->onDisconnect([](void *arg, AsyncClient *c)
                         {
        auto* self = static_cast<WiFiManager*>(arg);
        Serial.println("iPhone disconnected");
        if (self->_client == c) {
            self->_client        = nullptr;
            self->_waitingForPong = false;
            if (self->_onDisconnected) self->_onDisconnected();
        }
        delete c; }, this);

    client->onError([](void *arg, AsyncClient *c, int8_t error)
                    {
        auto* self = static_cast<WiFiManager*>(arg);
        Serial.printf("Client error: %d\n", error);
        if (self->_client == c) {
            self->_client        = nullptr;
            self->_waitingForPong = false;
            if (self->_onDisconnected) self->_onDisconnected();
        } }, this);

    client->onTimeout([](void *arg, AsyncClient *c, uint32_t time)
                      {
        Serial.printf("Client TCP timeout at %u ms\n", time);
        c->close(); }, this);
}

void WiFiManager::dropClient(const char *reason)
{
    Serial.printf("[WiFi] Dropping client: %s\n", reason);
    _waitingForPong = false;
    if (_client)
        _client->close();
}

void WiFiManager::processBackChannel(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        if (_bcLen == 0)
        {
            if (b != BC_MAGIC)
                continue;
        }
        if (_bcLen < sizeof(_bcBuf))
        {
            _bcBuf[_bcLen++] = b;
        }
        else
        {
            Serial.println("[BackChannel] Buffer overrun — resync");
            _bcLen = 0;
            continue;
        }
        if (_bcLen < 3)
            continue;

        uint16_t frameLen = (uint16_t)_bcBuf[1] | ((uint16_t)_bcBuf[2] << 8);
        size_t totalSize = 3 + frameLen;
        if (frameLen < 1 || totalSize > sizeof(_bcBuf))
        {
            Serial.printf("[BackChannel] Invalid len=%d — resync\n", frameLen);
            _bcLen = 0;
            continue;
        }
        if (_bcLen < totalSize)
            continue;

        uint8_t cmd = _bcBuf[3];
        const uint8_t *payload = (_bcLen > 4) ? &_bcBuf[4] : nullptr;
        size_t payloadLen = (frameLen > 1) ? frameLen - 1 : 0;

        switch (cmd)
        {
        case BC_CMD_PONG:
            _waitingForPong = false;
            _loggedThresholds = 0;
            Serial.println("[WiFi] PONG received");
            if (!_firstPongReceived)
            {
                _firstPongReceived = true;
                if (_onFirstPong)
                    _onFirstPong();
            }
            break;
        case BC_CMD_KEY1:
            Serial.println("[BackChannel] KEY1 — GFX Test 1");
            if (_keyCallback)
                _keyCallback('1');
            break;
        case BC_CMD_KEY2:
            Serial.println("[BackChannel] KEY2 — GFX Test 2");
            if (_keyCallback)
                _keyCallback('2');
            break;
        default:
            if (_dataCallback && payloadLen > 0)
                _dataCallback(payload, payloadLen);
            break;
        }
        _bcLen = 0;
    }
}