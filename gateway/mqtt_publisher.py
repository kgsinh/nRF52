"""
mqtt_publisher.py — MQTT client for publishing AssetTracker events.

Topics published:
    asset_tracker/shock        live shock notifications
    asset_tracker/sensor       live IMU readings
    asset_tracker/log_entry    historical entries from readback
    asset_tracker/status       connection state + counts
"""

import json
import time
import paho.mqtt.client as mqtt


BROKER_HOST = "localhost"
BROKER_PORT = 1883
CLIENT_ID   = "asset_tracker_gateway"

TOPIC_SHOCK      = "asset_tracker/shock"
TOPIC_SENSOR     = "asset_tracker/sensor"
TOPIC_LOG_ENTRY  = "asset_tracker/log_entry"
TOPIC_STATUS     = "asset_tracker/status"


class MQTTPublisher:
    def __init__(self, host: str = BROKER_HOST, port: int = BROKER_PORT):
        self._client = mqtt.Client(client_id=CLIENT_ID)
        self._client.on_connect    = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._host = host
        self._port = port
        self._connected = False

    def connect(self) -> bool:
        try:
            self._client.connect(self._host, self._port, keepalive=60)
            self._client.loop_start()
            time.sleep(0.5)   # give the loop time to complete CONNACK
            return self._connected
        except Exception as e:
            print(f"[mqtt] Connection failed: {e}")
            return False

    def disconnect(self):
        self._client.loop_stop()
        self._client.disconnect()

    def publish_shock(self, data: dict):
        self._publish(TOPIC_SHOCK, data)

    def publish_sensor(self, data: dict):
        self._publish(TOPIC_SENSOR, data)

    def publish_log_entry(self, data: dict):
        self._publish(TOPIC_LOG_ENTRY, data)

    def publish_status(self, data: dict):
        self._publish(TOPIC_STATUS, data)

    def _publish(self, topic: str, data: dict):
        if not self._connected:
            print(f"[mqtt] Not connected — dropping message on {topic}")
            return
        payload = json.dumps(data)
        self._client.publish(topic, payload, qos=1)
        print(f"[mqtt] → {topic}: {payload}")

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self._connected = True
            print(f"[mqtt] Connected to {self._host}:{self._port}")
        else:
            print(f"[mqtt] Connection refused: rc={rc}")

    def _on_disconnect(self, client, userdata, rc):
        self._connected = False
        print(f"[mqtt] Disconnected: rc={rc}")
