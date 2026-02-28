#include <Arduino.h>
#include <SPI.h>

#include "gps/GPSModule.h"
#include "telemetry/TelemetryPacket.h"
#include "display/DisplayModule.h"

#include "ble/BLEManager.h"
#include "ble/BleGraphicsTransport.h"
#include "graphics/Graphics.h"
#include "graphics/Activetransport.h"

#include <Adafruit_ST7735.h>
#include "Adafruit_iPhone/Adafruit_iPhoneTFT.h"

#include "demos/GFXTest.h"
#include "demos/GFXOrientTest.h"

#include "WiFi/WiFiManager.h"
#include "WiFi/WiFiTransport.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Tuning constants — adjust here, visible to all subsystems
// ─────────────────────────────────────────────────────────────────────────────

// Display dimensions
static constexpr uint16_t DISP_W = 128;
static constexpr uint16_t DISP_H = 160;

// Rotation (0-3, Adafruit convention)
static constexpr uint8_t ROT_TFT = 1;
static constexpr uint8_t ROT_PHONE = 1;

// Heartbeat timing (shared by BLE and WiFi)
// ESP32 sends a binary ping every PING_INTERVAL_MS.
// WiFi drops connection if no pong in PONG_TIMEOUT_MS.
// BLE logs a warning but lets the BLE stack manage disconnection.
static constexpr uint32_t PING_INTERVAL_MS = 3000;
static constexpr uint32_t PONG_TIMEOUT_MS = 9000;

// Auto-flush: if graphics code never calls flush() explicitly, accumulated
// bytes are sent after this many ms. Keep > single frame render time (~10ms),
// < PING_INTERVAL_MS. Mirror GFXAutoFlushIntervalSeconds on iPhone side.
static constexpr uint32_t AUTO_FLUSH_MS = 50;

// ─────────────────────────────────────────────────────────────────────────────
//  Hardware
// ─────────────────────────────────────────────────────────────────────────────
#define TFT_CS 5
#define TFT_DC 2
#define TFT_RST 4

GPSModule gps(16, 17);
Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
DisplayModule displayTFT(tft);

// ─────────────────────────────────────────────────────────────────────────────
//  Transport managers
// ─────────────────────────────────────────────────────────────────────────────
BLEManager ble;
static BleGraphicsTransport bleTransport(ble);

const char *ssid = "lotz_net1";
const char *password = "thelotznetwork1";
WiFiManager wifiManager(ssid, password, "esp32-gps");
WiFiTransport wifiTransport(wifiManager);

// ─────────────────────────────────────────────────────────────────────────────
//  Single graphics pipeline — ActiveTransport switches between BLE and WiFi
// ─────────────────────────────────────────────────────────────────────────────
ActiveTransport transport;
static Graphics gfx(transport);
static Adafruit_iPhoneTFT iphone_tft(gfx, (int16_t)DISP_W, (int16_t)DISP_H);
static DisplayModule displayPhone(iphone_tft);
static bool phoneReady = false;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static void initTftUI()
{
    displayTFT.begin(ROT_TFT);
}

static void initPhoneUI()
{
    iphone_tft.begin(0x0000);
    displayPhone.begin(ROT_PHONE);
    iphone_tft.flush();
    phoneReady = true;
}

static void runTestsOnAvailableDisplays(uint8_t which)
{
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

    if (phoneReady && transport.canSend())
    {
        if (which == '1')
        {
            runGFXTest(iphone_tft, DISP_W, DISP_H, 700);
        }
        else if (which == '2')
        {
            runGFXOrientTest(iphone_tft, 3000);
        }
        iphone_tft.flush();
        initPhoneUI();
    }

    initTftUI();
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

// ─────────────────────────────────────────────────────────────────────────────
//  Active transport selection
//  WiFi takes priority when connected (typically faster, lower latency).
//  Falls back to BLE if WiFi client disconnects.
//  Returns true if the active transport changed.
// ─────────────────────────────────────────────────────────────────────────────
static bool updateActiveTransport()
{
    GraphicsTransport *desired = nullptr;
    const char *name = "none";

    if (wifiTransport.canSend())
    {
        desired = &wifiTransport;
        name = "WiFi";
    }
    else if (bleTransport.canSend())
    {
        desired = &bleTransport;
        name = "BLE";
    }

    if (desired != transport.get())
    {
        transport.set(desired);
        if (desired)
        {
            Serial.printf("[Transport] Active: %s\n", name);
        }
        else
        {
            Serial.println("[Transport] No active transport");
        }
        return true; // transport changed
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  setup
// ─────────────────────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    Serial.println("\nBoot. Type 1 for GFX test, 2 for Orient test.");

    // Apply tuning constants
    wifiTransport.setAutoFlushMs(AUTO_FLUSH_MS);

    gps.begin();
    SPI.begin(18, -1, 23, TFT_CS);
    tft.initR(INITR_BLACKTAB);
    initTftUI();

    bleTransport.begin();
    wifiManager.begin();
}

// ─────────────────────────────────────────────────────────────────────────────
//  loop
// ─────────────────────────────────────────────────────────────────────────────
void loop()
{
    // ── Service transport managers ────────────────────────────────────────────
    ble.pump_BLE_txQ();
    wifiManager.tick(PING_INTERVAL_MS, PONG_TIMEOUT_MS);
    wifiTransport.tick();

    // ── Heartbeat ticks ───────────────────────────────────────────────────────
    ble.tick(PING_INTERVAL_MS, PONG_TIMEOUT_MS);

    // ── Update active transport, re-init UI if it changed ────────────────────
    bool changed = updateActiveTransport();
    bool canRender = transport.canSend();

    if (changed)
    {
        if (canRender)
        {
            Serial.println("Phone connected — waiting for TCP buffer...");
        }
        else
        {
            Serial.println("Phone disconnected.");
            phoneReady = false;
        }
    }

    // Deferred init — don't send init data until AsyncTCP's write buffer is ready.
    // On reconnect, space() briefly returns 0 even though connected() is true.
    // Sending before space() > 0 causes add() to fail silently → iPhone gets
    // a partial/empty init sequence → closes connection → reconnect loop.
    // BLE inits immediately — CoreBluetooth guarantees readiness before canSend().
    if (!phoneReady && canRender)
    {
        bool bufferReady = (transport.get() == &bleTransport) || (wifiManager.clientSpace() > 0);
        if (bufferReady)
        {
            Serial.printf("Buffer ready — initialising phone UI...\n");
            initPhoneUI();
        }
    }

    pollConsole();

    // ── GPS + telemetry render ────────────────────────────────────────────────
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

        displayTFT.renderTelemetry(pkt);

        if (phoneReady && canRender)
        {
            displayPhone.renderTelemetry(pkt);
            iphone_tft.flush();
        }
    }
}