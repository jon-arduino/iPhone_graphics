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

// Accumulate GFX bytes — nothing sent to TCP until flush() or auto-flush.
void WiFiTransport::send(const uint8_t *data, uint16_t len)
{
    if (!canSend() || len == 0)
        return;
    _txBuf.insert(_txBuf.end(), data, data + len);
    _lastSendMs = millis(); // reset auto-flush timer on every write
}

// Explicit flush — called by Graphics::flush() at logical frame boundaries.
// Sends entire accumulated buffer as ONE TCP write.
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
        // Not connected — discard stale frame rather than accumulate indefinitely
        Serial.printf("[WiFiTransport] flush discarded %d bytes (not connected)\n",
                      (int)_txBuf.size());
    }

    _txBuf.clear();
    _lastSendMs = 0; // clear timer — nothing pending
}

// Call from loop() every iteration.
// Triggers auto-flush if bytes have been sitting longer than _autoFlushMs
// without an explicit flush() arriving. Catches graphics code that never
// calls flush() and prevents the buffer from stalling indefinitely.
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