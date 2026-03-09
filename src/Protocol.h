#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Protocol.h — shared constants for the ESP32 ↔ iPhone wire protocol
//
//  TWO INDEPENDENT BYTE STREAMS share the same physical link (BLE or WiFi):
//
//  ┌─────────────────────────────────────────────────────────────────────────┐
//  │  GFX STREAM          ESP32 → iPhone     unframed, length-prefixed       │
//  │  BACK-CHANNEL        iPhone → ESP32     independently framed            │
//  └─────────────────────────────────────────────────────────────────────────┘
//
//  They never mix.  ESP32 never sends BC frames.  iPhone never sends GFX ops.
//  Ping/pong straddles both: ESP32 sends GFX_CMD_PING, iPhone replies BC_CMD_PONG.
// ─────────────────────────────────────────────────────────────────────────────

// ═════════════════════════════════════════════════════════════════════════════
//  GFX STREAM  (ESP32 → iPhone)
//  Opcode space: 0x00–0xEF  drawing commands
//                0xF0–0xFF  transport control / reserved
// ═════════════════════════════════════════════════════════════════════════════

static constexpr uint8_t GFX_CMD_PING = 0xF0; // ESP32 → iPhone: heartbeat request
                                              // iPhone replies with BC_CMD_PONG

// ═════════════════════════════════════════════════════════════════════════════
//  BACK-CHANNEL  (iPhone → ESP32)
//
//  Frame format:
//    [BC_MAGIC 0xA5][lenLow][lenHigh][cmd][payload...]
//
//    lenLow/lenHigh  = uint16_t little-endian count of bytes AFTER the 3-byte header
//                      i.e. len = 1 (cmd only) + sizeof(payload)
//    cmd             = one of the BC_CMD_* constants below
//    payload         = zero or more bytes depending on cmd (see per-cmd notes)
//
//  Opcode allocation:
//    0xF0–0xF1  transport (ping/pong)
//    0xF2–0xF3  hardware key stubs (GFX test buttons, legacy)
//    0xF4–0xF9  reserved
//    0x10–0x1F  touch events (populated below)
//    0x20–0xEF  reserved for future UI events (encoder, gesture, etc.)
// ═════════════════════════════════════════════════════════════════════════════

static constexpr uint8_t BC_MAGIC = 0xA5;

// ── Transport ─────────────────────────────────────────────────────────────────
static constexpr uint8_t BC_CMD_PING = 0xF0; // reserved — future iPhone→ESP32 ping
static constexpr uint8_t BC_CMD_PONG = 0xF1; // iPhone → ESP32: reply to GFX_CMD_PING
                                             // payload: none

// ── Legacy hardware key stubs ─────────────────────────────────────────────────
static constexpr uint8_t BC_CMD_KEY1 = 0xF2; // GFX test button 1  payload: none
static constexpr uint8_t BC_CMD_KEY2 = 0xF3; // GFX test button 2  payload: none

// ── Touch events ─────────────────────────────────────────────────────────────
//
//  The iPhone sends coordinates already mapped to virtual display space
//  (0,0)–(displayWidth-1, displayHeight-1).  The ESP32 side needs no remapping.
//
//  These mirror the Adafruit_TouchScreen / TSPoint API so that any sketch
//  written for a resistive touchscreen can be ported without changing loop():
//
//    TSPoint p = iPhoneTouch.getPoint();   // returns latest received point
//    if (p.z > MINPRESSURE) {              // z>0 on DOWN/MOVE, z=0 on UP
//        btn.press(btn.contains(p.x, p.y));
//    } else {
//        btn.press(false);
//    }
//
//  Payload layout for DOWN and MOVE (5 bytes total after cmd):
//    [xHigh][xLow][yHigh][yLow][z]
//    x, y  = uint16_t big-endian, display coordinates
//    z     = uint8_t  simulated pressure: 0xFF on contact, 0 on release
//            (capacitive screens have no real pressure — we use a fixed value
//             well above any MINPRESSURE threshold used in Adafruit examples)
//
//  Payload for UP: none (len=1, cmd only)

static constexpr uint8_t BC_CMD_TOUCH_DOWN = 0x10; // finger placed   payload: x(2) y(2) z(1)
static constexpr uint8_t BC_CMD_TOUCH_MOVE = 0x11; // finger dragging  payload: x(2) y(2) z(1)
static constexpr uint8_t BC_CMD_TOUCH_UP = 0x12;   // finger lifted    payload: none

// Simulated pressure value sent on DOWN and MOVE.
// Must be > MINPRESSURE (typically 10–100) used in Adafruit example sketches.
static constexpr uint8_t BC_TOUCH_PRESSURE = 0xFF;

// ── Payload sizes (bytes after cmd byte) ─────────────────────────────────────
static constexpr uint8_t BC_TOUCH_PAYLOAD_LEN = 5; // x(2) + y(2) + z(1)
static constexpr uint8_t BC_NO_PAYLOAD_LEN = 0;