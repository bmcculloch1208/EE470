
import os
import requests
import paho.mqtt.client as mqtt

# MQTT configuration
BROKER = "broker.hivemq.com"
PORT = 8000
TOPIC = "testtopic/temp/outTopic/bmm/pot"

# Your existing website
API_URL = "https://iot.bmm.solutions/mqtt_insert.php"
API_KEY = os.environ["MQTT_INGEST_KEY"]

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to HiveMQ!")
        client.subscribe(TOPIC)
        print(f"Subscribed to: {TOPIC}")
    else:
        print(f"Connection failed: {reason_code}")

def on_message(client, userdata, msg):
    try:
        value = int(msg.payload.decode().strip())

        if not 0 <= value <= 1023:
            print("Invalid ADC value:", value)
            return

        print(f"MQTT Received: {value}")

        response = requests.post(
            API_URL,
            json={"potentiometer": value},
            headers={"X-API-Key": API_KEY},
            timeout=10
        )

        response.raise_for_status()

        print("Database insert successful:", value)

    except ValueError:
        print("Invalid MQTT payload")

    except requests.RequestException as e:
        print("Database insert failed:", e)

client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2,
    transport="websockets"
)

client.on_connect = on_connect
client.on_message = on_message

print("Connecting to HiveMQ...")
client.connect(BROKER, PORT, 60)

client.loop_forever()

