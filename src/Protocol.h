#pragma once
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────────────────
//  GFX Stream opcodes  (ESP32 → iPhone, unframed length-prefixed binary stream)
//  0x00–0xEF  drawing commands (see GFXOp enum on iPhone side)
//  0xF0–0xFF  reserved for transport control
//  iPhone never sends these. ESP32 is the sole sender.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr uint8_t GFX_CMD_PING = 0xF0; // ESP32 → iPhone: heartbeat request

// ─────────────────────────────────────────────────────────────────────────────
//  Back-Channel opcodes  (iPhone → ESP32, independently framed)
//  Framing: [0xA5 magic][lenLow][lenHigh][cmd][payload...]
//
//  This is a completely independent protocol from the GFX stream.
//  ESP32 never sends BC frames; iPhone never sends GFX opcodes.
//
//  Ping/pong straddle both protocols:
//    ping  sent by ESP32 via GFX stream  (GFX_CMD_PING 0xF0)
//    pong  sent by iPhone via back-channel (BC_CMD_PONG 0xF1)
//  BC_CMD_PING reserved for a future reverse-ping from iPhone if needed.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr uint8_t BC_MAGIC = 0xA5;
static constexpr uint8_t BC_CMD_PING = 0xF0; // reserved — future iPhone → ESP32 ping
static constexpr uint8_t BC_CMD_PONG = 0xF1; // iPhone → ESP32: heartbeat response
static constexpr uint8_t BC_CMD_KEY1 = 0xF2; // iPhone → ESP32: GFX Test 1
static constexpr uint8_t BC_CMD_KEY2 = 0xF3; // iPhone → ESP32: GFX Test 2
                                             // 0xF4–0xFF: available for future back-channel commands