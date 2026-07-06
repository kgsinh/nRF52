"""
gateway.py — AssetTracker BLE → MQTT → InfluxDB gateway.

Usage:
    python gateway.py

Requires:
    pip install -r requirements.txt
    Mosquitto running on localhost:1883
    InfluxDB 1.8 running on localhost:8086 (database: asset_tracker)
"""

import asyncio
import json
import time
from datetime import datetime, timezone

from ble_client import AssetTrackerClient
from mqtt_publisher import MQTTPublisher
from decoder import ShockEvent, SensorReading, LogEntry


# ── InfluxDB direct write (optional — set USE_INFLUXDB = False to skip) ──
USE_INFLUXDB = True
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB   = "asset_tracker"

if USE_INFLUXDB:
    try:
        from influxdb import InfluxDBClient
        influx = InfluxDBClient(host=INFLUXDB_HOST,
                                port=INFLUXDB_PORT,
                                database=INFLUXDB_DB)
        influx.create_database(INFLUXDB_DB)
        print(f"[influx] Connected to {INFLUXDB_HOST}:{INFLUXDB_PORT} "
              f"db={INFLUXDB_DB}")
    except Exception as e:
        print(f"[influx] Failed to connect: {e} — disabling InfluxDB writes")
        USE_INFLUXDB = False


def write_to_influx(measurement: str, fields: dict, tags: dict = None):
    """Write one measurement point to InfluxDB."""
    if not USE_INFLUXDB:
        return
    try:
        point = {
            "measurement": measurement,
            "time":        datetime.now(tz=timezone.utc).isoformat(),
            "fields":      fields,
        }
        if tags:
            point["tags"] = tags
        influx.write_points([point])
    except Exception as e:
        print(f"[influx] Write error: {e}")


# ── MQTT publisher ────────────────────────────────────────────────────
mqtt = MQTTPublisher()


# ── Callbacks ─────────────────────────────────────────────────────────
def on_shock(event: ShockEvent):
    """Live shock notification received."""
    data = event.to_dict()

    # Publish to MQTT
    mqtt.publish_shock(data)

    # Write to InfluxDB
    write_to_influx(
        measurement="shock_event",
        fields={
            "magnitude": event.magnitude_ms2,
            "accel_x":   event.accel_x_ms2,
            "accel_y":   event.accel_y_ms2,
            "accel_z":   event.accel_z_ms2,
        },
        tags={"source": "live"}
    )


def on_sensor(reading: SensorReading):
    """Live IMU notification received."""
    mqtt.publish_sensor(reading.to_dict())
    # Not written to InfluxDB by default — high frequency, use selectively
    # Uncomment to store sensor data:
    # write_to_influx("sensor", reading.to_dict())


def on_log_entry(entry: LogEntry):
    """Historical log entry received via readback."""
    data = entry.to_dict()
    mqtt.publish_log_entry(data)

    write_to_influx(
        measurement="shock_event",
        fields={
            "magnitude": entry.magnitude_ms2,
            "accel_x":   entry.accel_x_ms2,
            "accel_y":   entry.accel_y_ms2,
            "accel_z":   entry.accel_z_ms2,
            "log_index": float(entry.index),
        },
        tags={"source": "historical"}
    )


def on_status(data: dict):
    """BLE connection state change."""
    mqtt.publish_status(data)
    print(f"[status] {data}")


# ── Main ──────────────────────────────────────────────────────────────
async def main():
    print("=" * 50)
    print("  AssetTracker Gateway")
    print("=" * 50)

    # Connect to MQTT broker
    if not mqtt.connect():
        print("[gateway] MQTT connection failed — check Mosquitto is running")
        print("  Start with: docker-compose up -d mosquitto")
        return

    # Start BLE client
    callbacks = {
        "on_shock":     on_shock,
        "on_sensor":    on_sensor,
        "on_log_entry": on_log_entry,
        "on_status":    on_status,
    }

    ble = AssetTrackerClient(callbacks)

    try:
        await ble.run_forever()
    except KeyboardInterrupt:
        print("\n[gateway] Shutting down...")
    finally:
        ble.stop()
        mqtt.disconnect()
        print("[gateway] Done.")


if __name__ == "__main__":
    asyncio.run(main())
