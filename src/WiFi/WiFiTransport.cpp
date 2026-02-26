#include "WiFiTransport.h"

WiFiTransport::WiFiTransport(WiFiManager& wifi)
    : _wifi(wifi)
{}

void WiFiTransport::begin()
{
    // WiFiManager::begin() handles everything.
    // Kept for interface symmetry.
}

bool WiFiTransport::canSend() const
{
    return _wifi.isConnected() && _wifi.clientConnected();
}

void WiFiTransport::send(const uint8_t* data, uint16_t len)
{
    if (canSend()) {
        _wifi.send(data, len);
    } else {
        Serial.printf("WiFiTransport::send skipped (wifi=%d client=%d)\n",
                      _wifi.isConnected(), _wifi.clientConnected());
    }
} 