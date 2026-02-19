#include <Arduino.h>
#include "TelemetryPrinter.h"

void printTelemetry(const TelemetryPacket &pkt)
{
    Serial.println("----- Telemetry Packet -----");

    Serial.print("Lat:       ");
    Serial.println(pkt.lat, 6);

    Serial.print("Lng:       ");
    Serial.println(pkt.lng, 6);

    Serial.print("Alt (m):   ");
    Serial.println(pkt.alt, 2);

    Serial.print("Speed:     ");
    Serial.println(pkt.speed, 2);

    Serial.print("Course:    ");
    Serial.println(pkt.course, 2);

    Serial.print("V-North:   ");
    Serial.println(pkt.vNorth, 3);

    Serial.print("V-East:    ");
    Serial.println(pkt.vEast, 3);

    Serial.print("Climb:     ");
    Serial.println(pkt.climb, 3);

    Serial.print("Sats:      ");
    Serial.println(pkt.sats);

    Serial.print("HDOP:      ");
    Serial.println(pkt.hdop, 2);

    Serial.print("Timestamp: ");
    Serial.println(pkt.timestamp);

    Serial.println("----------------------------");
}