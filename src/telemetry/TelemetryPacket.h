#pragma once
#include <stdint.h>

struct TelemetryPacket
{
    float lat;
    float lng;
    float alt;
    float speed;
    float course;
    float vNorth;
    float vEast;
    float climb;
    uint8_t sats;
    float hdop;
    uint32_t timestamp;
} __attribute__((packed));