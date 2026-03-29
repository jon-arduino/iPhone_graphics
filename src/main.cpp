#include <Arduino.h>
#include <SPI.h>

#include "gps/GPSModule.h"
#include "telemetry/TelemetryPacket.h"
#include "display/DisplayModule.h"

#include "ble/BLEManager.h"
#include "ble/BleGraphicsTransport.h"
#include "graphics/Graphics.h"
#include "graphics/ActiveTransport.h"

#include <Adafruit_ST7735.h>
#include "Adafruit_iPhone/Adafruit_iPhoneTFT.h"

#include "demos/GFXTest.h"

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
//  Hardware Dissplay (tft) and GPS
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

// SoftAP fallback -- ESP32 hosts its own network when home network unavailable.
// Also triggered by console 'w' command for demos away from home network.
static const char *AP_SSID = "ESP32-RemoteUI";
static const char *AP_PASSWORD = "remoteui123";

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
volatile bool bleSubscribePending = false;
volatile bool wifiReconnectPending = false; // set by onConnected, consumed by loop()
static volatile uint8_t pendingTest = 0;    // set by key callback (AsyncTCP task), consumed by loop()

// ── GPS status tracking ───────────────────────────────────────────────────────
// Used to show an informative "waiting for GPS" page instead of blank fields.
enum class GpsState
{
    Absent,
    Acquiring,
    Fixed
};
static GpsState gpsState = GpsState::Absent;
static uint32_t gpsLastUpdateMs = 0;  // millis() of last hasNewData() == true
static uint8_t gpsLastSats = 0;       // most recent satellite count
static uint32_t gpsStatusSentMs = 0;  // last time we sent a status page
static uint32_t transportReadyMs = 0; // millis() when transport last became active

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
    bool wasReady = phoneReady;
    phoneReady = false;

    // Both displays driven simultaneously. If iPhone not connected, pass tft
    // as both args so the test runs on TFT alone without branching.
    Adafruit_GFX &phoneDisp = (wasReady && transport.canSend())
                                  ? static_cast<Adafruit_GFX &>(iphone_tft)
                                  : static_cast<Adafruit_GFX &>(tft);

    if (which == '1')
    {
        Serial.println("Running GFXTest (TFT + iPhone simultaneous)...");
        runGFXTest(tft, phoneDisp, DISP_W, DISP_H, 700);
    }
    else if (which == '2')
    {
        Serial.println("Running GFXOrientTest (TFT + iPhone simultaneous)...");
        runGFXOrientTest(tft, phoneDisp, 3000);
    }

    if (wasReady && transport.canSend())
    {
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
        pendingTest = (uint8_t)c;
    }
    else if (c == 'w' || c == 'W')
    {
        Serial.printf("[WiFi] Switching to SoftAP -- SSID: %s  password: %s\n",
                      AP_SSID, AP_PASSWORD);
        wifiManager.switchToSoftAP();
    }
    else
    {
        Serial.println("Unknown command. Type 1 for GFX test, 2 for Orient test, W for SoftAP.");
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
    Serial.println("\nBoot. Type 1 for GFX test, 2 for Orient test, W to switch to SoftAP WiFi.");

    // Apply tuning constants
    wifiTransport.setAutoFlushMs(AUTO_FLUSH_MS);

    gps.begin();
    SPI.begin(18, -1, 23, TFT_CS);
    tft.initR(INITR_BLACKTAB);
    initTftUI();

    bleTransport.begin();
    wifiManager.setSoftAP(AP_SSID, AP_PASSWORD); // fallback if home network absent
    wifiManager.begin();

    wifiManager.setHeartbeat(PING_INTERVAL_MS, PONG_TIMEOUT_MS);

    // Background task — calls update() on both managers every 100ms.
    // Each manager owns its ping timing and write serialisation via mutex.
    // No transport calls needed in loop() — loop() can block freely.
    xTaskCreate([](void *)
                {
        for (;;) {
            wifiManager.update();
            ble.update();
            vTaskDelay(pdMS_TO_TICKS(100));
        } }, "heartbeat", 2048, nullptr, 1, nullptr);

    // Clear WiFiTransport buffer on disconnect — prevents stale bytes from a
    // previous connection prepending to the next connection's init sequence,
    // which causes immediate disconnect/reconnect loops on reconnect.
    // Both onConnected and onDisconnected fire on the AsyncTCP task.
    // We must NOT touch _txBuf there — it races with loop()'s renderTelemetry().
    // Instead set a flag; loop() resets the transport and re-inits UI safely.
    wifiManager.onConnected([]()
                            { wifiReconnectPending = true; });

    wifiManager.onDisconnected([]()
                               {
                                   // nothing — handled via transport change detection in loop()
                               });

    // BLE init gate — initPhoneUI() must wait until iPhone subscribes to the
    // TX characteristic. Firing before subscribe means bytes queue up and
    // blast out all at once on subscribe, causing the iPhone to immediately
    // unsubscribe and disconnect (reason 531).
    ble.onSubscribed([](bool ready)
                     {
        if (ready) {
            Serial.println("[BLE] subscribed — ready to init phone UI");
            // Set a flag so loop() calls initPhoneUI() on the main task.
            // Don't call initPhoneUI() directly here — this fires on the
            // NimBLE task, not loop(), and would race with loop()'s GFX writes.
            extern volatile bool bleSubscribePending;
            bleSubscribePending = true;
        } else {
            Serial.println("[BLE] unsubscribed");
        } });

    // Key commands from iPhone back-channel arrive on the AsyncTCP task.
    // Set pendingTest so loop() runs the test safely on the main task,
    // where it can block without starving the heartbeat or interleaving
    // with telemetry rendering.
    auto keyHandler = [](uint8_t key)
    {
        pendingTest = key;
    };
    wifiManager.onKey(keyHandler);
    ble.onKey(keyHandler);
}

// ─────────────────────────────────────────────────────────────────────────────
//  loop
// ─────────────────────────────────────────────────────────────────────────────
void loop()
{
    // ── Service WiFi GFX transport ────────────────────────────────────────────
    // Only auto-flush when phone UI is initialised — prevents stale telemetry
    // bytes flushing into a fresh TCP connection before the init frame goes out.
    if (phoneReady)
        wifiTransport.tick();

    // ── Update active transport, re-init UI if it changed ────────────────────
    bool changed = updateActiveTransport();
    bool canRender = transport.canSend();

    // Handle WiFi reconnect-without-disconnect — onConnected sets this flag
    // from the AsyncTCP task. We act on it here in loop() where it's safe to
    // reset the transport buffer and force a clean re-init sequence.
    if (wifiReconnectPending)
    {
        wifiReconnectPending = false;
        wifiTransport.reset();
        phoneReady = false;
        transportReadyMs = millis();
        Serial.println("WiFi client reconnected — resetting transport.");
    }

    if (changed)
    {
        if (canRender)
        {
            transportReadyMs = millis();
            Serial.println("Phone connected — waiting for TCP buffer ready...");
        }
        else
        {
            Serial.println("Phone disconnected.");
            phoneReady = false;
            wifiTransport.reset();
        }
    }

    // Deferred UI init — wait until TCP write buffer has space before sending
    // init data. On reconnect the buffer may briefly show zero space even though
    // the connection is established, causing an immediate disconnect if we write
    // too early. BLE inits immediately (CoreBluetooth guarantees ready state).
    if (!phoneReady && canRender)
    {
        bool isBLE = (transport.get() == &bleTransport);
        bool isWiFi = !isBLE;

        // WiFi: wait until TCP write buffer has space AND a short settle time
        // has elapsed since the transport became active. The iPhone app needs
        // a moment to finish its own connection setup before we send the init
        // frame — sending too early causes an immediate disconnect.
        // BLE: wait until iPhone subscribes to TX characteristic.
        uint32_t settleMs = isBLE ? 0 : 200; // WiFi needs ~200ms settle, BLE handles via subscribe
        bool settled = (millis() - transportReadyMs) >= settleMs;
        bool bufferReady = (isWiFi && wifiManager.clientSpace() > 0 && settled) || (isBLE && bleSubscribePending);

        if (bufferReady)
        {
            if (isWiFi)
                Serial.printf("TCP buffer ready (%d bytes) — initialising phone UI...\n",
                              (int)wifiManager.clientSpace());
            bleSubscribePending = false;
            initPhoneUI();
        }
    }

    pollConsole();

    // ── Pending GFX test (deferred from key callback / serial) ────────────────
    // Consumed here on the main task — safe to block loop() for the test duration.
    if (pendingTest)
    {
        uint8_t which = pendingTest;
        pendingTest = 0;
        runTestsOnAvailableDisplays(which);
        return; // skip telemetry this iteration — initPhoneUI() already flushed
    }

    // ── GPS + telemetry render ────────────────────────────────────────────────
    gps.update();
    if (gps.hasNewData())
    {
        const GPSData &d = gps.getData();
        gpsLastUpdateMs = millis();
        gpsLastSats = d.sats;

        // Determine fix quality from satellite count and HDOP
        GpsState prevState = gpsState;
        if (d.sats >= 3 && d.hdop < 5.0f)
            gpsState = GpsState::Fixed;
        else if (d.sats > 0)
            gpsState = GpsState::Acquiring;
        else
            gpsState = GpsState::Absent;

        // Transition to Fixed — clear status page and redraw telemetry labels.
        // initPhoneUI() redraws the label frame; renderTelemetry() below fills data.
        if (gpsState == GpsState::Fixed && prevState != GpsState::Fixed)
        {
            if (phoneReady && canRender)
            {
                Serial.println("[GPS] Fix acquired — reinitialising phone UI");
                initPhoneUI(); // clears screen, redraws labels, sets phoneReady=true
            }
        }
        // Also handle initial connect while already fixed (phoneReady just became true)
        // — covered by initPhoneUI() in the connect path, nothing extra needed here.

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
    else
    {
        // No new GPS data — update state and send a status page periodically
        uint32_t now = millis();
        uint32_t silenceMs = now - gpsLastUpdateMs;

        // Downgrade state if GPS has gone quiet.
        // gpsLastUpdateMs==0 means we've never had data — stay Absent, don't
        // flip through Acquiring based on a meaningless large silenceMs.
        if (gpsLastUpdateMs > 0)
        {
            if (silenceMs > 5000 && gpsState == GpsState::Fixed)
                gpsState = GpsState::Acquiring;
            if (silenceMs > 15000)
                gpsState = GpsState::Absent;
        }

        // Send status page every 2s — only when not fixed
        if (gpsState != GpsState::Fixed && phoneReady && canRender && (now - gpsStatusSentMs >= 2000))
        {
            gpsStatusSentMs = now;

            uint32_t uptimeSec = now / 1000;

            iphone_tft.fillScreen(0x0000);
            iphone_tft.setTextSize(2);

            if (gpsState == GpsState::Absent)
            {
                iphone_tft.setTextColor(0xF800); // red
                iphone_tft.setCursor(4, 4);
                iphone_tft.println("GPS");
                iphone_tft.println("Not Found");
            }
            else
            {
                iphone_tft.setTextColor(0xFFE0); // yellow
                iphone_tft.setCursor(4, 4);
                iphone_tft.println("Acquiring");
                iphone_tft.println("GPS Fix...");
            }

            iphone_tft.setTextSize(1);
            iphone_tft.setTextColor(0x7BEF); // grey
            iphone_tft.setCursor(4, 52);

            if (gpsLastSats > 0)
            {
                iphone_tft.print("Sats visible: ");
                iphone_tft.println(gpsLastSats);
            }
            else
            {
                iphone_tft.println("No satellites yet");
            }

            iphone_tft.setCursor(4, 64);
            iphone_tft.print("Uptime: ");
            if (uptimeSec >= 60)
            {
                iphone_tft.print(uptimeSec / 60);
                iphone_tft.print("m ");
            }
            iphone_tft.print(uptimeSec % 60);
            iphone_tft.println("s");

            if (gpsLastUpdateMs > 0)
            {
                iphone_tft.setCursor(4, 76);
                iphone_tft.print("Last fix: ");
                iphone_tft.print(silenceMs / 1000);
                iphone_tft.println("s ago");
            }

            iphone_tft.flush();
        }
    }
}