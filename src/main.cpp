#include <Arduino.h>
#include "gps/GPSModule.h"
#include "ble/BLEManager.h"
#include "ble/BleGraphicsTransport.h"
#include "telemetry/TelemetryPacket.h"
#include "telemetry/TelemetryPrinter.h"
#include "display/DisplayModule.h"
#include "demos/GFXTest.h"
#include "demos/GFXOrientTest.h"
#include "graphics/Graphics.h"


GPSModule gps(16, 17);
DisplayModule display;

BLEManager ble;
static BleGraphicsTransport transport(ble);
static Graphics gfx(transport);

static bool waitForIPhone(uint32_t timeoutMs = 20000)
{
    uint32_t start = millis();
    while (millis() - start < timeoutMs)
    {
        if (ble.canSend())
            return true; // connected + notify subscribed
        delay(50);
    }
    return false;
}

void setup()
{
    Serial.begin(115200);
    gps.begin();
    display.begin();

    // IMPORTANT: Only init BLE once (transport.begin() calls ble.begin()).
    transport.begin();
}

void loop()
{
    ble.pump_BLE_txQ();
    // rest of your app...
 //   gps.update();

    // Run the graphics test once after iPhone becomes ready
    static bool ranTestThisConnection = false;

    if (!ranTestThisConnection && ble.canSend())
    {
        Serial.println("iPhone subscribed — running GFX test...");
        runGFXTest(gfx, 256, 128, 700);
        runGFXOrientTest(gfx, 256, 128, 3000);
        ranTestThisConnection = true;
    }

    // If not connected yet, optionally wait once (but don't do it every loop)
    static bool didInitialWait = false;
    if (!didInitialWait)
    {
        didInitialWait = true;
        if (!waitForIPhone())
        {
            Serial.println("Timed out waiting for iPhone subscribe (will keep running anyway).");
        }
    }

    /* 
    // Normal telemetry flow
    if (gps.hasNewData())
    {
        const GPSData &d = gps.getData();

        double courseRad = d.course * (PI / 180.0);
        double vNorth = d.speed * cos(courseRad);
        double vEast = d.speed * sin(courseRad);

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

        // Send over BLE (only sends if subscribed)
       // ble.sendTelemetry(pkt);

        // printTelemetry(pkt);
        display.renderTelemetry(pkt);
    } */

    // Detect disconnect and allow test again on next connection
    if (!ble.canSend())
    {
        ranTestThisConnection = false;
    }

}
