/*
 * Title: ESP8266 LiPo Voltage Monitor
 *
 * Description:
 * Reads the voltage of a single cell LiPo battery through a
 * resistor divider connected to A0. The divider scales the
 * battery voltage so it remains within the ADC input range.
 *
 * Divider:
 * Battery + ---- R_TOP ---- A0 ---- R_BOTTOM ---- GND
 *
 * The program averages multiple ADC readings and reports
 * the measured values once per minute over Serial.
 *
 * Author: Brandon McCulloch
 * 
 * Version: 1.0
 */

#include <Arduino.h>

const int VOLTAGE_PIN = A0;

// Divider resistor values in ohms
const float R_TOP = 9860.0;
const float R_BOTTOM = 29530.0;

// ESP8266 ADC characteristics
const float ADC_REFERENCE = 3.3;
const float ADC_COUNTS = 1023.0;

// Calibration multiplier based on measured ADC error
const float CALIBRATION_FACTOR = 0.9395;

// Number of readings used for averaging
const int READ_COUNT = 20;

// Sampling period: 60 seconds
const unsigned long READ_INTERVAL_MS = 60000;

unsigned long lastReadTime = 0;
unsigned long readingIndex = 0;


float readAverageADC() {

    float total = 0.0;

    for (int sample = 0; sample < READ_COUNT; sample++) {
        total += analogRead(VOLTAGE_PIN);
        delay(10);
    }

    return total / READ_COUNT;
}


void setup() {

    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("ESP8266 LiPo Voltage Monitor");
    Serial.println("Reading,Minutes,ADC_Count,A0_Voltage,Battery_Voltage");

    // Allows the first measurement to occur immediately
    lastReadTime = millis() - READ_INTERVAL_MS;
}


void loop() {

    unsigned long now = millis();

    if (now - lastReadTime < READ_INTERVAL_MS) {
        return;
    }

    lastReadTime = now;
    readingIndex++;

    // Get averaged ADC value
    float rawADC = readAverageADC();

    // Convert ADC count into voltage measured at A0
    float inputVoltage =
        (rawADC / ADC_COUNTS) *
        ADC_REFERENCE *
        CALIBRATION_FACTOR;

    // Undo the resistor divider to determine battery voltage
    float dividerRatio =
        (R_TOP + R_BOTTOM) / R_BOTTOM;

    float calculatedBatteryVoltage =
        inputVoltage * dividerRatio;

    // Time since program startup in minutes
    float runtimeMinutes =
        now / 60000.0;

    // CSV output
    Serial.print(readingIndex);
    Serial.print(",");

    Serial.print(runtimeMinutes, 2);
    Serial.print(",");

    Serial.print(rawADC, 1);
    Serial.print(",");

    Serial.print(inputVoltage, 3);
    Serial.print(",");

    Serial.println(calculatedBatteryVoltage, 3);
}
