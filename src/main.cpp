#include <Arduino.h>
#include <SPI.h>

#include "gps/GPSModule.h"
#include "telemetry/TelemetryPacket.h"
#include "display/DisplayModule.h"

#include "ble/BLEManager.h"
#include "ble/BleGraphicsTransport.h"
#include "graphics/Graphics.h"

#include <Adafruit_ST7735.h>
#include "Adafruit_iPhone/Adafruit_iPhoneTFT.h" // adjust include path if needed

#include "demos/GFXTest.h"
#include "demos/GFXOrientTest.h"

static constexpr uint16_t DISP_W = 128;
static constexpr uint16_t DISP_H = 160;

// ST7735 pins (your existing wiring)
#define TFT_CS 5
#define TFT_DC 2
#define TFT_RST 4

// Choose rotations you want for each target
static constexpr uint8_t ROT_TFT = 1;
static constexpr uint8_t ROT_PHONE = 1;

// ---- Modules ----
GPSModule gps(16, 17);

// Real TFT
static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

// iPhone graphics pipeline
BLEManager ble;
static BleGraphicsTransport transport(ble);
static Graphics gfx(transport);
static Adafruit_iPhoneTFT iphone_tft(gfx, (int16_t)DISP_W, (int16_t)DISP_H);

// DisplayModules now take Adafruit_GFX&
static DisplayModule displayTFT(tft);
static DisplayModule displayPhone(iphone_tft);

// Track whether phone is currently initialized with labels, etc.
static bool phoneReady = false;

// Optional helper: wait once at boot
static bool waitForIPhone(uint32_t timeoutMs = 20000)
{
    uint32_t start = millis();
    while (millis() - start < timeoutMs)
    {
        if (ble.canSend())
            return true;
        delay(50);
    }
    return false;
}

static void initPhoneUI()
{
    // Allocate remote surface + clear it
    iphone_tft.begin(0x0000);      // black
    displayPhone.begin(ROT_PHONE); // draws banner + static labels
    iphone_tft.flush();
    phoneReady = true;
}

static void initTftUI()
{
    displayTFT.begin(ROT_TFT);
}

static void runTestsOnAvailableDisplays(uint8_t which)
{
    // Run on TFT always
    if (which == '1')
    {
        Serial.println("Running GFXTest on TFT...");
        runGFXTest(tft, DISP_W, DISP_H, 700);
    }
    else if (which == '2')
    {
        Serial.println("Running GFXOrientTest on TFT...");
        runGFXOrientTest(tft, 3000);
    }

    // Run on phone only if ready
    if (phoneReady && ble.canSend())
    {
        if (which == '1')
        {
            Serial.println("Running GFXTest on iPhone...");
            runGFXTest(iphone_tft, DISP_W, DISP_H, 700);
        }
        else if (which == '2')
        {
            Serial.println("Running GFXOrientTest on iPhone...");
            runGFXOrientTest(iphone_tft, 3000);
        }
        iphone_tft.flush();
    }

    // After tests, restore telemetry screens (labels)
    Serial.println("Restoring telemetry UI...");
    initTftUI();
    if (ble.canSend())
    {
        initPhoneUI();
    }
}

static void pollConsole()
{
    if (!Serial.available())
        return;

    int c = Serial.read();
    if (c == '\n' || c == '\r')
        return;

    if (c == '1' || c == '2')
    {
        runTestsOnAvailableDisplays((uint8_t)c);
    }
    else
    {
        Serial.println("Unknown command. Type 1 for GFX test, 2 for Orient test.");
    }

    // Drain any extra characters on the line
    while (Serial.available())
    {
        int d = Serial.peek();
        if (d == '\n' || d == '\r')
        {
            Serial.read();
            break;
        }
        Serial.read();
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println("Boot. Type 1 for GFX test, 2 for Orient test.");

    gps.begin();

    // Init TFT hardware
    SPI.begin(18, -1, 23, TFT_CS);
    tft.initR(INITR_BLACKTAB);
    initTftUI();

    // Init BLE transport once
    transport.begin();

    // Optional: wait briefly for iPhone at boot (but keep running regardless)
   /* if (!waitForIPhone())
    {
        Serial.println("Timed out waiting for iPhone subscribe (will keep running anyway).");
    }
    */  // don't bother waiting for iPhone since it can be turned on/off at any time, and we handle that in loop()
}

void loop()
{
    // Keep BLE draining often
    ble.pump_BLE_txQ();

    // Handle connect / disconnect edges
    static bool lastCanSend = false;
    bool nowCanSend = ble.canSend();

    if (nowCanSend && !lastCanSend)
    {
        Serial.println("iPhone subscribed — initializing iPhone UI...");
        initPhoneUI();
    }
    else if (!nowCanSend && lastCanSend)
    {
        Serial.println("iPhone disconnected/unsubscribed.");
        phoneReady = false;
    }
    lastCanSend = nowCanSend;

    // Console commands
    pollConsole();

    // Telemetry update
    gps.update();
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
            if (dt > 0.0)
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

        // Always local TFT
        displayTFT.renderTelemetry(pkt);

        // iPhone only when ready
        if (phoneReady && nowCanSend)
        {
            displayPhone.renderTelemetry(pkt);
            iphone_tft.flush();
        }
    }
}
