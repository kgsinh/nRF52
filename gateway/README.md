# AssetTracker Gateway

Python BLE → MQTT → InfluxDB gateway for the AssetTracker nRF52 firmware.

## Prerequisites

- Python 3.9+
- Docker + Docker Compose
- Bluetooth adapter on your PC (built-in or USB dongle)
- nRF52-DK flashed with AssetTracker firmware

## Quick start

### 1. Start the infrastructure stack

From the project root (where `docker-compose.yml` is):

```bash
docker-compose up -d
```

This starts:
- **Mosquitto** (MQTT broker) on `localhost:1883`
- **InfluxDB 1.8** on `localhost:8086` — database `asset_tracker` created automatically
- **Grafana** on `localhost:3000` — login: `admin` / `admin`

### 2. Install Python dependencies

```bash
cd gateway
pip install -r requirements.txt
```

### 3. Run the gateway

```bash
python gateway.py
```

The gateway will:
1. Connect to MQTT broker
2. Scan for `AssetTracker` over BLE
3. Connect when found
4. Enable notifications on all characteristics
5. Read flash status
6. Trigger log readback (all stored events stream immediately)
7. Listen for live shock notifications

### 4. View the dashboard

Open `http://localhost:3000` in your browser.
Navigate to **Dashboards → AssetTracker → Shock Log**.

The dashboard shows:
- **Shock magnitude timeline** — every event as a spike
- **Acceleration XYZ** — directional breakdown per event
- **Total events** stat
- **Peak magnitude** stat
- **Historical vs live** pie chart — the gap-fill demo

---

## MQTT topics

| Topic | Content |
|---|---|
| `asset_tracker/shock` | Live shock event (JSON) |
| `asset_tracker/sensor` | Live IMU reading (JSON) |
| `asset_tracker/log_entry` | Historical entry from readback (JSON) |
| `asset_tracker/status` | Connection state changes |

Monitor all topics live:
```bash
mosquitto_sub -h localhost -t "asset_tracker/#" -v
```

---

## Data format

### Shock event (live + historical)
```json
{
  "timestamp_ms": 28514,
  "accel_x": -156.906,
  "accel_y": 137.566,
  "accel_z": 156.901,
  "magnitude": 261.08
}
```

### Log entry (historical, includes index/total)
```json
{
  "index": 0,
  "total": 5,
  "timestamp_ms": 28514,
  "accel_x": -156.906,
  "accel_y": 137.566,
  "accel_z": 156.901,
  "magnitude": 261.08
}
```

---

## Architecture

```
nRF52 AssetTracker
    │  BLE GATT notifications
    ▼
gateway.py  (asyncio)
    ├── ble_client.py     scan + connect + notify handlers
    ├── decoder.py        binary struct → Python dataclasses
    └── mqtt_publisher.py JSON → MQTT

Mosquitto MQTT broker
    │  topic routing
    ▼
InfluxDB 1.8    (time-series storage)
    │  InfluxQL queries
    ▼
Grafana         (dashboard + alerting)
```

---

## Disabling InfluxDB

Set `USE_INFLUXDB = False` in `gateway.py` to run gateway-only
(MQTT publish still works without InfluxDB).
