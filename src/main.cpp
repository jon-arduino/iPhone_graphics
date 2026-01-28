#include <Arduino.h>
#include "GPSModule.h"
#include "BLEManager.h"
#include "TelemetryPacket.h"
#include "TelemetryPrinter.h"
#include "DisplayModule.h"

GPSModule gps(16, 17);
BLEManager ble;
DisplayModule display;

void setup()
{
    Serial.begin(115200);
    gps.begin();
    ble.begin();
    display.begin();
}

void loop()
{
    gps.update();

    if (gps.hasNewData())
    {
        const GPSData &d = gps.getData();

        // Compute velocity components
        double courseRad = d.course * (PI / 180.0);
        double vNorth = d.speed * cos(courseRad);
        double vEast = d.speed * sin(courseRad);

        // Compute climb rate
        static double prevAlt = d.alt;
        static uint32_t prevTime = d.timestamp;
        double climb = 0.0;

        if (d.timestamp > prevTime)
        {
            double dt = (d.timestamp - prevTime) / 1000.0;
            climb = (d.alt - prevAlt) / dt;
        }

        prevAlt = d.alt;
        prevTime = d.timestamp;

        // Fill telemetry packet
        TelemetryPacket pkt;
        pkt.lat = d.lat;
        pkt.lng = d.lng;
        pkt.alt = d.alt;
        pkt.speed = d.speed;
        pkt.course = d.course;
        pkt.vNorth = vNorth;
        pkt.vEast = vEast;
        pkt.climb = climb;
        pkt.sats = d.sats;
        pkt.hdop = d.hdop;
        pkt.timestamp = d.timestamp;

        // Send over BLE
        ble.sendTelemetry(pkt);
        printTelemetry(pkt);
        display.renderTelemetry(pkt);
    }
}