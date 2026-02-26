#pragma once

#include "graphics/Graphics.h"
#include "WiFiManager.h"

class WiFiTransport : public GraphicsTransport
{
public:
    explicit WiFiTransport(WiFiManager& wifi);
    void begin()                                  override;
    bool canSend()                          const  override;
    void send(const uint8_t* data, uint16_t len)  override;

private:
    WiFiManager& _wifi;
};