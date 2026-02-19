#pragma once
#include <TinyGPSPlus.h>

/*
 * GPSData
 * -------
 * A simple POD (plain old data) struct that holds the most recent
 * parsed GPS values. This is the "public interface" for the rest
 * of the avionics system — SD logging, BLE telemetry, UI, etc.
 *
 * All values are updated atomically when TinyGPS++ reports a new fix.
 */
struct GPSData
{
    double lat = 0.0;
    double lng = 0.0;
    double alt = 0.0;
    double speed = 0.0;  // km/h
    double course = 0.0; // degrees (0–360)
    uint8_t sats = 0;
    double hdop = 0.0;
    bool valid = false;
    unsigned long timestamp = 0;
};

/*
 * GPSModule
 * ---------
 * Encapsulates:
 *   - UART communication with the GPS module
 *   - Feeding TinyGPS++ with raw NMEA bytes
 *   - Producing clean, typed GPSData objects
 *
 * This class hides all parsing and serial details so the rest of the
 * system can treat GPS as a simple data source.
 */
class GPSModule
{
public:
    // Constructor: specify RX/TX pins and baud rate (default 9600 for Adafruit GPS)
    GPSModule(int rxPin, int txPin, uint32_t baud = 9600);

    // Initialize UART and prepare GPS module
    void begin();

    // Call frequently in loop() to process incoming NMEA data
    void update();

    // Returns true once per new GPS fix
    bool hasNewData();

    // Returns the most recent parsed GPS data
    const GPSData &getData() const;

private:
    int _rx, _tx;           // UART pin assignments
    uint32_t _baud;         // GPS baud rate
    HardwareSerial _serial; // ESP32 UART instance (UART2)
    TinyGPSPlus _gps;       // TinyGPS++ parser instance
    GPSData _data;          // Latest parsed GPS data
    bool _newData = false;  // Flag indicating fresh data available
    static const int SPEED_SAMPLES = 4;
    double _speedBuf[SPEED_SAMPLES];
    double _courseBuf[SPEED_SAMPLES];
    int _sampleIndex = 0;
    bool _bufferFilled = false;
};