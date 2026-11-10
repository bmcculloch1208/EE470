
/*
 * Project: EE 470 ESP8266 IoT Sensor System
 * Version: 2.0
 *
 * Inputs:
 *   D1 - Pushbutton (Node_1)
 *   D5 - Tilt switch (Node_2)
 *   D6 - HC-SR04 TRIG
 *   D7 - HC-SR04 ECHO
 *   A0 - Photoresistor
 *
 * Outputs:
 *   Serial monitoring and HTTPS sensor transmission
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// -------------------------------------------
// Configuration
// -------------------------------------------

const char* WIFI_SSID = "BMM Network";
const char* WIFI_PASSWORD = "roundjungle285";

const char* SERVER_URL =
    "https://iot.bmm.solutions/insert.php";

const int BUTTON_PIN = D1;
const int TILT_PIN = D5;
const int TRIG_PIN = D6;
const int ECHO_PIN = D7;
const int LDR_PIN = A0;

// Change to LOW if your tilt module activates LOW.
const int TILT_ACTIVE_STATE = HIGH;

const unsigned long DEBOUNCE_MS = 60;
const unsigned long RETRY_MS = 30000;
const unsigned long WIFI_RECONNECT_MS = 10000;

// -------------------------------------------
// Time zones
// -------------------------------------------

const char* timeZones[] = {
    "America/New_York",
    "America/Chicago",
    "America/Denver",
    "America/Los_Angeles",
    "America/Anchorage",
    "Pacific/Honolulu",
    "America/Puerto_Rico"
};

const char* timeZoneNames[] = {
    "Eastern", "Central", "Mountain",
    "Pacific", "Alaska", "Hawaii", "Atlantic"
};

String selectedTimeZone = "America/Los_Angeles";

// -------------------------------------------
// Measurement and switch structures
// -------------------------------------------

struct Measurement {
    bool used = false;
    String nodeId;
    String timestamp;
    float distanceCm = 0;
    int lightValue = 0;
    unsigned long lastAttempt = 0;
    bool attempted = false;
};

struct SwitchState {
    int pin;
    int activeState;
    int stableState;
    int lastRawState;
    unsigned long lastChange;
};

const int QUEUE_SIZE = 8;
Measurement queueData[QUEUE_SIZE];

SwitchState buttonSwitch = {
    BUTTON_PIN, LOW, HIGH, HIGH, 0
};

SwitchState tiltSwitch = {
    TILT_PIN, TILT_ACTIVE_STATE, LOW, LOW, 0
};

// -------------------------------------------
// Wi-Fi
// -------------------------------------------

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi");

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected!");

        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        Serial.print("MAC Address: ");
        Serial.println(WiFi.macAddress());

        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        Serial.println("WiFi connection failed");
    }
}

void maintainWiFi() {
    static unsigned long lastCheck = 0;

    if (WiFi.status() == WL_CONNECTED) return;

    if (millis() - lastCheck >= WIFI_RECONNECT_MS) {
        lastCheck = millis();
        Serial.println("Reconnecting WiFi...");
        WiFi.reconnect();
    }
}

// -------------------------------------------
// Time-zone selection
// -------------------------------------------

void selectTimeZone() {
    Serial.println();
    Serial.println("====== TIME ZONES ======");
    Serial.println("1. Eastern");
    Serial.println("2. Central");
    Serial.println("3. Mountain");
    Serial.println("4. Pacific");
    Serial.println("5. Alaska");
    Serial.println("6. Hawaii");
    Serial.println("7. Atlantic");
    Serial.println();
    Serial.println("Select 1-7 (default Pacific):");

    unsigned long start = millis();

    while (millis() - start < 10000) {
        if (Serial.available()) {
            char choice = Serial.read();

            if (choice >= '1' && choice <= '7') {
                int index = choice - '1';
                selectedTimeZone = timeZones[index];

                Serial.print("Selected: ");
                Serial.println(timeZoneNames[index]);

                while (Serial.available()) Serial.read();
                return;
            }
        }
        delay(10);
    }

    Serial.println("Default: Pacific");
}

// -------------------------------------------
// Get current timestamp from API
// -------------------------------------------

String getCurrentTime() {
    if (WiFi.status() != WL_CONNECTED) {
        return "";
    }

    WiFiClientSecure client;
    client.setInsecure(); // Lab testing only

    HTTPClient http;
    http.setTimeout(7000);

    String url =
        "https://timeapi.io/api/Time/current/zone"
        "?timeZone=" + selectedTimeZone;

    if (!http.begin(client, url)) return "";

    int code = http.GET();
    String timestamp = "";

    if (code == HTTP_CODE_OK) {
        JsonDocument doc;

        if (!deserializeJson(doc, http.getString())) {
            String raw = doc["dateTime"].as<String>();

            // Normalize to 6 fractional digits for
            // MariaDB DATETIME(6).
            if (raw.length() >= 19) {
                timestamp = raw.substring(0, 19);

                String fraction = "000000";
                int dot = raw.indexOf('.');

                if (dot >= 0) {
                    fraction = raw.substring(dot + 1);

                    if (fraction.length() > 6)
                        fraction = fraction.substring(0, 6);

                    while (fraction.length() < 6)
                        fraction += "0";
                }

                timestamp += "." + fraction;
            }
        }
    }

    http.end();
    return timestamp;
}

// -------------------------------------------
// Sensor readings
// -------------------------------------------

float readDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration =
        pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) return -1.0;

    return duration * 0.0343 / 2.0;
}

int readLight() {
    return analogRead(LDR_PIN);
}

// -------------------------------------------
// Switch initialization and debounce
// -------------------------------------------

void initializeSwitch(SwitchState &sw) {
    sw.lastRawState = digitalRead(sw.pin);
    sw.stableState = sw.lastRawState;
    sw.lastChange = millis();
}

// Returns true only on a new activation.
bool newActivation(SwitchState &sw) {
    int raw = digitalRead(sw.pin);

    if (raw != sw.lastRawState) {
        sw.lastRawState = raw;
        sw.lastChange = millis();
    }

    if (millis() - sw.lastChange >= DEBOUNCE_MS &&
        raw != sw.stableState) {

        sw.stableState = raw;
        return raw == sw.activeState;
    }

    return false;
}

// -------------------------------------------
// Create measurement on switch activation
// -------------------------------------------

void captureMeasurement(const String &nodeId) {
    int slot = -1;

    for (int i = 0; i < QUEUE_SIZE; i++) {
        if (!queueData[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        Serial.println("ERROR: Message queue full!");
        return;
    }

    Measurement m;

    m.nodeId = nodeId;
    m.distanceCm = readDistance();
    m.lightValue = readLight();

    // Keep the original values for retransmission.
    m.timestamp = getCurrentTime();

    if (m.timestamp.length() == 0) {
        Serial.println("ERROR: Could not get timestamp");
        Serial.println("Measurement not transmitted");
        return;
    }

    if (m.distanceCm < 0) {
        Serial.println("ERROR: No ultrasonic echo");
        return;
    }

    m.used = true;
    queueData[slot] = m;

    Serial.println();
    Serial.println("===== NEW MEASUREMENT =====");
    Serial.println("Node: " + m.nodeId);
    Serial.println("Timestamp: " + m.timestamp);

    Serial.print("Distance: ");
    Serial.print(m.distanceCm, 1);
    Serial.println(" cm");

    Serial.print("Light: ");
    Serial.println(m.lightValue);
}

// -------------------------------------------
// URL-encode parameter values
// -------------------------------------------

String encodeValue(const String &value) {
    String result = "";
    const char* hex = "0123456789ABCDEF";

    for (unsigned int i = 0; i < value.length(); i++) {
        unsigned char c = value[i];

        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.') {
            result += (char)c;
        } else {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }

    return result;
}

// -------------------------------------------
// HTTPS transmission
// -------------------------------------------

// Expected server response:
// {"status":"inserted"}
// or
// {"status":"duplicate"}

bool sendMeasurement(const Measurement &m) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi offline: retry later");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Replace with certificate validation later

    HTTPClient http;
    http.setTimeout(7000);

    if (!http.begin(client, SERVER_URL)) {
        return false;
    }

    http.addHeader(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    String body =
        "nodeId=" + encodeValue(m.nodeId) +
        "&nodeDistance=" +
        encodeValue(String(m.distanceCm, 2)) +
        "&nodeLight=" +
        encodeValue(String(m.lightValue)) +
        "&timestamp=" +
        encodeValue(m.timestamp);

    Serial.println("Sending " + m.nodeId + "...");

    int code = http.POST(body);
    String response = http.getString();

    Serial.print("HTTP status: ");
    Serial.println(code);

    Serial.print("Response: ");
    Serial.println(response);

    bool delivered = false;

    if (code == HTTP_CODE_OK ||
        code == HTTP_CODE_CREATED ||
        code == HTTP_CODE_CONFLICT) {

        JsonDocument doc;

        if (!deserializeJson(doc, response)) {
            String status =
                doc["status"].as<String>();

            delivered =
                (status == "inserted" ||
                 status == "duplicate");
        }
    }

    http.end();
    return delivered;
}

// -------------------------------------------
// Process queued transmissions and retries
// -------------------------------------------

void processQueue() {
    for (int i = 0; i < QUEUE_SIZE; i++) {
        Measurement &m = queueData[i];

        if (!m.used) continue;

        unsigned long now = millis();

        if (m.attempted &&
            now - m.lastAttempt < RETRY_MS) {
            continue;
        }

        m.attempted = true;
        m.lastAttempt = now;

        if (sendMeasurement(m)) {
            Serial.println("Delivery confirmed!");
            m.used = false;
        } else {
            Serial.println(
                "Delivery not confirmed. Retry in 30s."
            );
        }

        // Process one message per loop iteration.
        break;
    }
}

// -------------------------------------------
// Monitor switch inputs
// -------------------------------------------

void checkSwitches() {
    if (newActivation(buttonSwitch)) {
        captureMeasurement("node_1");
    }

    if (newActivation(tiltSwitch)) {
        captureMeasurement("node_2");
    }
}

// -------------------------------------------
// Setup
// -------------------------------------------

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(TILT_PIN, INPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    initializeSwitch(buttonSwitch);
    initializeSwitch(tiltSwitch);

    connectWiFi();
    selectTimeZone();

    Serial.println("EE470 system ready!");
    Serial.println("Press button or activate tilt.");
}

// -------------------------------------------
// Main loop
// -------------------------------------------

void loop() {
    maintainWiFi();
    checkSwitches();
    processQueue();

    delay(10);
}

