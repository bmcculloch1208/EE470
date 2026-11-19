
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ------------------------------------
// Wi-Fi Configuration
// ------------------------------------
const char* WIFI_SSID = "Brandon";
const char* WIFI_PASSWORD = "Stanly@200";

// ------------------------------------
// MQTT Configuration
// ------------------------------------
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char* MQTT_OUT_TOPIC =
    "testtopic/temp/inTopic/bmm";

const char* MQTT_IN_TOPIC =
    "testtopic/temp/inTopic/bmm";

const char* MQTT_POT_TOPIC =
    "testtopic/temp/outTopic/bmm/pot";

// MQTT clients
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ------------------------------------
// Pin Assignments
// ------------------------------------
const int LED_PIN = D5;
const int BUTTON_PIN = D6;
const int POT_PIN = A0;

// ------------------------------------
// Timing
// ------------------------------------
const unsigned long PRINT_INTERVAL = 500;
const unsigned long PUBLISH_INTERVAL = 15000;
const unsigned long RECONNECT_INTERVAL = 5000;
const unsigned long BUTTON_DELAY = 5000;
const unsigned long DEBOUNCE_INTERVAL = 40;

unsigned long lastPrintTime = 0;
unsigned long lastPublishTime = 0;
unsigned long lastReconnectTime = 0;

// Button tracking
int lastButtonReading = LOW;
int stableButtonState = LOW;
unsigned long lastButtonChange = 0;

// Button MQTT event
bool buttonEventPending = false;
bool buttonResetPending = false;
unsigned long buttonPublishTime = 0;

// ------------------------------------
// MQTT Callback (Incoming Messages)
// ------------------------------------
void mqttCallback(char* topic,
                  byte* payload,
                  unsigned int length) {

    String message = "";

    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.print("MQTT Received: ");
    Serial.println(message);

    if (strcmp(topic, MQTT_IN_TOPIC) == 0) {

        if (message == "1") {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("LED ON via MQTT");
        }
        else if (message == "0") {
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED OFF via MQTT");
        }
    }
}

// ------------------------------------
// MQTT Connection
// ------------------------------------
void connectMQTT() {

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (mqtt.connected()) {
        return;
    }

    if (millis() - lastReconnectTime < RECONNECT_INTERVAL) {
        return;
    }

    lastReconnectTime = millis();

    String clientId =
        "ESP8266-" + String(ESP.getChipId(), HEX);

    Serial.println("Connecting to MQTT...");

    if (mqtt.connect(clientId.c_str())) {
        Serial.println("MQTT Connected!");

        // Subscribe to incoming LED commands
        if (mqtt.subscribe(MQTT_IN_TOPIC)) {
            Serial.print("Subscribed to: ");
            Serial.println(MQTT_IN_TOPIC);
        } else {
            Serial.println("MQTT subscribe failed");
        }
    }
    else {
        Serial.print("MQTT connection failed: ");
        Serial.println(mqtt.state());
    }
}

// ------------------------------------
// Setup
// ------------------------------------
void setup() {

    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT); // 10k pull-down

    digitalWrite(LED_PIN, LOW);

    delay(500);

    Serial.println();
    Serial.println("ESP8266 Ready!");

    // Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Connecting to Wi-Fi...");

    // MQTT configuration
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
}

// ------------------------------------
// Main Loop
// ------------------------------------
void loop() {

    unsigned long now = millis();

    // 1. Maintain MQTT connection
    connectMQTT();

    if (mqtt.connected()) {
        mqtt.loop();
    }

    // 2. Serial commands for LED (Part A)
    while (Serial.available() > 0) {

        char command = Serial.read();

        if (command == '1') {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("LED ON via Serial");
        }
        else if (command == '0') {
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED OFF via Serial");
        }
    }

    // 3. Read button (active HIGH, pull-down)
    int reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonReading) {
        lastButtonChange = now;
    }

    if (now - lastButtonChange >= DEBOUNCE_INTERVAL &&
        reading != stableButtonState) {

        stableButtonState = reading;

        Serial.print("Button: ");
        Serial.println(stableButtonState);

        // Only trigger when button is RELEASED
        if (stableButtonState == LOW) {
            buttonEventPending = true;
        }
    }

    lastButtonReading = reading;

    // 4. Publish button event after release
    if (buttonEventPending && mqtt.connected()) {

        if (mqtt.publish(MQTT_OUT_TOPIC, "1")) {

            Serial.println("Button MQTT Published: 1");

            buttonPublishTime = now;
            buttonResetPending = true;
            buttonEventPending = false;
        }
    }

    // 5. Publish button reset after 5 seconds
    if (buttonResetPending &&
        now - buttonPublishTime >= BUTTON_DELAY &&
        mqtt.connected()) {

        if (mqtt.publish(MQTT_OUT_TOPIC, "0")) {
            Serial.println("Button MQTT Published: 0");
            buttonResetPending = false;
        }
    }

    // 6. Display potentiometer voltage
    if (now - lastPrintTime >= PRINT_INTERVAL) {

        lastPrintTime = now;

        int potValue = analogRead(POT_PIN);

        float voltage = potValue * (3.3 / 1023.0);

        Serial.print("Voltage: ");
        Serial.print(voltage, 2);
        Serial.println(" V");
    }

    // 7. Publish potentiometer every 15 seconds
    
    // 7. Publish potentiometer every 15 seconds
    
    // 7. Publish potentiometer every 15 seconds
    if (now - lastPublishTime >= PUBLISH_INTERVAL) {

        lastPublishTime = now;

        int potValue = analogRead(POT_PIN);
        String payload = String(potValue);

        Serial.println();
        Serial.println("===== MQTT DEBUG =====");

        Serial.print("WiFi connected: ");
        Serial.println(WiFi.status() == WL_CONNECTED ? "YES" : "NO");

        Serial.print("MQTT connected: ");
        Serial.println(mqtt.connected() ? "YES" : "NO");

        Serial.print("MQTT topic: ");
        Serial.println(MQTT_POT_TOPIC);

        Serial.print("ADC value: ");
        Serial.println(potValue);

        if (mqtt.connected()) {

            // Publish to original Part B/C topic
            bool originalSuccess = mqtt.publish(
                MQTT_OUT_TOPIC,
                payload.c_str()
            );

            // Publish to dedicated Part F topic
            bool potSuccess = mqtt.publish(
                MQTT_POT_TOPIC,
                payload.c_str()
            );

            Serial.print("Original topic publish: ");
            Serial.println(originalSuccess ? "SUCCESS" : "FAILED");

            Serial.print("Pot topic publish: ");
            Serial.println(potSuccess ? "SUCCESS" : "FAILED");

        } else {
            Serial.print("MQTT state: ");
            Serial.println(mqtt.state());
        }

        Serial.println("======================");
    }


    delay(10);
}
