#include "WiFiTransport.h"

WiFiTransport::WiFiTransport(WiFiManager &wifi)
    : _wifi(wifi)
{
    _txBuf.reserve(TX_BUF_RESERVE);
}

void WiFiTransport::begin()
{
    _txBuf.reserve(TX_BUF_RESERVE);
}

bool WiFiTransport::canSend() const
{
    return _wifi.isConnected() && _wifi.clientConnected();
}

void WiFiTransport::send(const uint8_t *data, uint16_t len)
{
    if (!canSend() || !data || len == 0)
        return;
    _txBuf.insert(_txBuf.end(), data, data + len);
    _lastSendMs = millis();
}

void WiFiTransport::flush()
{
    if (_txBuf.empty())
        return;

    if (canSend())
    {
        _wifi.send(_txBuf.data(), _txBuf.size());
    }
    else
    {
        Serial.printf("[WiFiTransport] flush discarded %d bytes (not connected)\n",
                      (int)_txBuf.size());
    }

    _txBuf.clear();
    _lastSendMs = 0;
}

void WiFiTransport::reset()
{
    _txBuf.clear();
    _lastSendMs = 0;
}

void WiFiTransport::tick()
{
    if (_txBuf.empty() || _lastSendMs == 0)
        return;

    if (millis() - _lastSendMs >= _autoFlushMs)
    {
        Serial.printf("[WiFiTransport] auto-flush %d bytes after %ums idle\n",
                      (int)_txBuf.size(), _autoFlushMs);
        flush();
    }
}