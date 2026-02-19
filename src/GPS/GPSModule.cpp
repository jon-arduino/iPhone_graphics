#include "GPSModule.h"
/*-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --*Helper : compute average of a circular buffer * -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --*/
    static double average(const double *buf, int size, bool filled, int index)
{
    int count = filled ? size : index;
    if (count <= 0)
        return 0.0;

    double sum = 0.0;
    for (int i = 0; i < count; i++)
        sum += buf[i];

    return sum / count;
}
/*
 * Constructor
 * Stores pin assignments and baud rate.
 * Initializes HardwareSerial on UART2 (ESP32 has UART0/1/2).
 */
GPSModule::GPSModule(int rxPin, int txPin, uint32_t baud)
    : _rx(rxPin), _tx(txPin), _baud(baud), _serial(2) {}

/*
 * begin()
 * -------
 * Initializes the UART interface used to communicate with the GPS module.
 * The Adafruit Ultimate GPS v3 defaults to 9600 baud.
 */
void GPSModule::begin()
{
    _serial.begin(_baud, SERIAL_8N1, _rx, _tx);
}

/*
 * update()
 * --------
 * Reads all available bytes from the GPS UART and feeds them into TinyGPS++.
 * When TinyGPS++ reports a new fix, we update our GPSData struct.
 *
 * This function should be called as frequently as possible (every loop).
 */
void GPSModule::update()
{
    while (_serial.available())
    {
        char c = _serial.read();
        _gps.encode(c); // TinyGPS++ processes one character at a time
    }

    // Always sample speed and course when available
    if (_gps.speed.isValid())
    {
        _speedBuf[_sampleIndex] = _gps.speed.kmph();
        _courseBuf[_sampleIndex] = _gps.course.deg();
        _sampleIndex++;

        if (_sampleIndex >= SPEED_SAMPLES)
        {
            _sampleIndex = 0;
            _bufferFilled = true;
        }
    }

    // Only update data when TinyGPS++ reports a new fix
    if (_gps.location.isUpdated())
    {
        _data.lat = _gps.location.lat();
        _data.lng = _gps.location.lng();
        _data.alt = _gps.altitude.meters();

        // Use averaged speed & course
        _data.speed = average(_speedBuf, SPEED_SAMPLES, _bufferFilled, _sampleIndex);
        _data.course = average(_courseBuf, SPEED_SAMPLES, _bufferFilled, _sampleIndex);

        _data.sats = _gps.satellites.value();
        _data.hdop = _gps.hdop.hdop();
        _data.valid = _gps.location.isValid();
        _data.timestamp = millis();
        _newData = true;
    }
}

/*
 * hasNewData()
 * ------------
 * Returns true exactly once per new GPS fix.
 * After returning true, the flag resets until the next update.
 */
bool GPSModule::hasNewData()
{
    if (_newData)
    {
        _newData = false;
        return true;
    }
    return false;
}

/*
 * getData()
 * ---------
 * Returns a const reference to the most recent GPSData struct.
 * The caller should treat this as read‑only.
 */
const GPSData &GPSModule::getData() const
{
    return _data;
}