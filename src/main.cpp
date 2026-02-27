#include <Arduino.h>
#include <SPI.h>

#include "GPS/GPSModule.h"
#include "telemetry/TelemetryPacket.h"
#include "display/DisplayModule.h"

#include "ble/BLEManager.h"
#include "ble/BleGraphicsTransport.h"
#include "graphics/Graphics.h"

#include <Adafruit_ST7735.h>
#include "Adafruit_iPhone/Adafruit_iphoneTFT.h"

#include "demos/GFXTest.h"
#include "demos/GFXOrientTest.h"

#include "WiFi/WiFiManager.h"
#include "WiFi/WiFiTransport.h"

static constexpr uint16_t DISP_W = 128;
static constexpr uint16_t DISP_H = 160;

#define TFT_CS  5
#define TFT_DC  2
#define TFT_RST 4

static constexpr uint8_t ROT_TFT   = 1;
static constexpr uint8_t ROT_PHONE = 1;

// ── Hardware TFT ──────────────────────────────────────────────────────────────
GPSModule gps(16, 17);
static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
static DisplayModule   displayTFT(tft);

// ── BLE graphics pipeline ─────────────────────────────────────────────────────
BLEManager ble;
static BleGraphicsTransport bleTransport(ble);
static Graphics             bleGfx(bleTransport);
static Adafruit_iPhoneTFT   ble_iphone_tft(bleGfx, (int16_t)DISP_W, (int16_t)DISP_H);
static DisplayModule        displayBlePhone(ble_iphone_tft);
static bool                 blePhoneReady = false;

// ── WiFi graphics pipeline ────────────────────────────────────────────────────
const char*   ssid     = "lotz_net1";
const char*   password = "thelotznetwork1";
WiFiManager   wifiManager(ssid, password, "esp32-gps");
WiFiTransport wifiTransport(wifiManager);
static Graphics           wifiGfx(wifiTransport);
static Adafruit_iPhoneTFT wifi_iphone_tft(wifiGfx, (int16_t)DISP_W, (int16_t)DISP_H);
static DisplayModule      displayWifiPhone(wifi_iphone_tft);
static bool               wifiPhoneReady = false;

// ── Helpers ───────────────────────────────────────────────────────────────────
static void initTftUI()        { displayTFT.begin(ROT_TFT); }

static void initBlePhoneUI()
{
    ble_iphone_tft.begin(0x0000);
    displayBlePhone.begin(ROT_PHONE);
    ble_iphone_tft.flush();
    blePhoneReady = true;
}

static void initWifiPhoneUI()
{
    wifi_iphone_tft.begin(0x0000);
    displayWifiPhone.begin(ROT_PHONE);
    wifi_iphone_tft.flush();
    wifiPhoneReady = true;
}

static void runTestsOnAvailableDisplays(uint8_t which)
{
    if (which == '1') { Serial.println("Running GFXTest on TFT..."); runGFXTest(tft, DISP_W, DISP_H, 500); }
    else if (which == '2') { Serial.println("Running GFXOrientTest on TFT..."); runGFXOrientTest(tft, 2000); }

    if (blePhoneReady && ble.canSend()) {
        if (which == '1') { runGFXTest(ble_iphone_tft, DISP_W, DISP_H, 700); }
        else if (which == '2') { runGFXOrientTest(ble_iphone_tft, 3000); }
        ble_iphone_tft.flush();
    }

    if (wifiPhoneReady && wifiTransport.canSend()) {
        if (which == '1') { runGFXTest(wifi_iphone_tft, DISP_W, DISP_H, 700); }
        else if (which == '2') { runGFXOrientTest(wifi_iphone_tft, 3000); }
        wifi_iphone_tft.flush();
    }

    initTftUI();
    if (ble.canSend())           initBlePhoneUI();
    if (wifiTransport.canSend()) initWifiPhoneUI();
}

static void pollConsole()
{
    if (!Serial.available()) return;
    int c = Serial.read();
    if (c == '\n' || c == '\r') return;
    if (c == '1' || c == '2') { runTestsOnAvailableDisplays((uint8_t)c); }
    else { Serial.println("Unknown command. Type 1 for GFX test, 2 for Orient test."); }
    while (Serial.available()) {
        int d = Serial.peek();
        if (d == '\n' || d == '\r') { Serial.read(); break; }
        Serial.read();
    }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    Serial.println("\nBoot. Type 1 for GFX test, 2 for Orient test.");

    gps.begin();
    SPI.begin(18, -1, 23, TFT_CS);
    tft.initR(INITR_BLACKTAB);
    initTftUI();

    bleTransport.begin();
    wifiManager.begin();

    wifiManager.onData([](const uint8_t* data, size_t len) {
        Serial.printf("[WiFi RX] %d bytes\n", len);
    });
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop()
{
    ble.pump_BLE_txQ();

    // WiFi heartbeat — sends ping every 3s, drops connection if no pong in 9s
    wifiManager.tick();

    // BLE edge detection
    static bool lastBleCanSend = false;
    bool nowBleCanSend = ble.canSend();
    if (nowBleCanSend && !lastBleCanSend) {
        Serial.println("BLE iPhone connected — initialising BLE UI...");
        initBlePhoneUI();
    } else if (!nowBleCanSend && lastBleCanSend) {
        Serial.println("BLE iPhone disconnected.");
        blePhoneReady = false;
    }
    lastBleCanSend = nowBleCanSend;

    // WiFi edge detection
    static bool lastWifiCanSend = false;
    bool nowWifiCanSend = wifiTransport.canSend();
    if (nowWifiCanSend && !lastWifiCanSend) {
        Serial.println("WiFi iPhone connected — initialising WiFi UI...");
        initWifiPhoneUI();
    } else if (!nowWifiCanSend && lastWifiCanSend) {
        Serial.println("WiFi iPhone disconnected.");
        wifiPhoneReady = false;
    }
    lastWifiCanSend = nowWifiCanSend;

    pollConsole();

    gps.update();
    if (gps.hasNewData())
    {
        const GPSData& d = gps.getData();

        double courseRad = d.course * (PI / 180.0);
        double vNorth    = d.speed * cos(courseRad);
        double vEast     = d.speed * sin(courseRad);

        static double   prevAlt  = d.alt;
        static uint32_t prevTime = d.timestamp;
        double climb = 0.0;
        if (d.timestamp > prevTime) {
            double dt = (d.timestamp - prevTime) / 1000.0;
            if (dt > 0.0) climb = (d.alt - prevAlt) / dt;
        }
        prevAlt  = d.alt;
        prevTime = d.timestamp;

        TelemetryPacket pkt;
        pkt.lat = d.lat;   pkt.lng = d.lng;     pkt.alt    = d.alt;
        pkt.speed = d.speed; pkt.course = d.course;
        pkt.vNorth = vNorth; pkt.vEast = vEast;  pkt.climb  = climb;
        pkt.sats = d.sats;   pkt.hdop  = d.hdop; pkt.timestamp = d.timestamp;

        displayTFT.renderTelemetry(pkt);

        if (blePhoneReady && nowBleCanSend) {
            displayBlePhone.renderTelemetry(pkt);
            ble_iphone_tft.flush();
        }

        // WiFi: identical pipeline to BLE — framed graphics commands over TCP
        if (wifiPhoneReady && nowWifiCanSend) {
            displayWifiPhone.renderTelemetry(pkt);
            wifi_iphone_tft.flush();
        }
    }
}
