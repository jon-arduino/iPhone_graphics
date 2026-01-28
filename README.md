#ESP32 Telemetry Module

A modular GPS telemetry system for RC aircraft, built on the ESP32.  
Features include:

- TinyGPS++ parsing
- Smoothed ground speed and course
- North/East velocity vector
- Climb rate (vertical velocity)
- Clean GPSData struct for logging and BLE telemetry
- Modular architecture for future expansion

## Project Structure

- `src/GPSModule.h` — GPS interface and data model  
- `src/GPSModule.cpp` — UART parsing, smoothing, fix handling  
- `src/main.cpp` — Telemetry output and velocity calculations  

## Current Status

- Stable GPS parsing  
- Averaged speed and course  
- V-north and V-east  
- Climb rate  
- Ready for BLE telemetry integration  

## Branching

- `main` — stable, tested code  
- feature branches — experimental work (e.g., `v-east-climb-rate`)  

## Requirements

- PlatformIO  
- ESP32 Dev Module  
- Adafruit Ultimate GPS v3 (or compatible NMEA GPS)

## BLE packet
struct TelemetryPacket {
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
};

