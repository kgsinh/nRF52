"""
decoder.py — Binary payload decoder for AssetTracker BLE packets.

Mirrors the packed C structs defined in ble_service.h exactly.
All multi-byte integers are little-endian (ARM default).
"""

import struct
from dataclasses import dataclass
from typing import Optional


# ── Struct formats ───────────────────────────────────────────────────
# Prefix '<' = little-endian

# Live shock notification — ble_shock_payload (18 bytes)
# int64 timestamp_ms, int16 accel_x/y/z, int32 magnitude
FMT_SHOCK = "<qhhhI"
SIZE_SHOCK = struct.calcsize(FMT_SHOCK)  # 18 bytes

# Live sensor notification — ble_sensor_payload (12 bytes)
# int16 accel_x/y/z, int16 gyro_x/y/z
FMT_SENSOR = "<hhhhhh"
SIZE_SENSOR = struct.calcsize(FMT_SENSOR)  # 12 bytes

# Log entry notification — ble_log_entry_payload (20 bytes)
# uint16 index, uint16 total, int64 timestamp_ms,
# int16 accel_x/y/z, uint16 magnitude
FMT_LOG_ENTRY = "<HHqhhhH"
SIZE_LOG_ENTRY = struct.calcsize(FMT_LOG_ENTRY)  # 20 bytes

# Flash status — ble_flash_status_payload (8 bytes)
# uint32 used_bytes, uint32 total_bytes
FMT_FLASH_STATUS = "<II"
SIZE_FLASH_STATUS = struct.calcsize(FMT_FLASH_STATUS)  # 8 bytes


# ── Data classes ─────────────────────────────────────────────────────
@dataclass
class ShockEvent:
    timestamp_ms: int      # firmware uptime in ms
    accel_x_ms2: float     # m/s² (decoded from × 1000 fixed-point)
    accel_y_ms2: float
    accel_z_ms2: float
    magnitude_ms2: float

    def to_dict(self) -> dict:
        return {
            "timestamp_ms":  self.timestamp_ms,
            "accel_x":       round(self.accel_x_ms2, 3),
            "accel_y":       round(self.accel_y_ms2, 3),
            "accel_z":       round(self.accel_z_ms2, 3),
            "magnitude":     round(self.magnitude_ms2, 3),
        }


@dataclass
class SensorReading:
    accel_x_ms2: float
    accel_y_ms2: float
    accel_z_ms2: float
    gyro_x_rads: float
    gyro_y_rads: float
    gyro_z_rads: float

    def to_dict(self) -> dict:
        return {
            "accel_x": round(self.accel_x_ms2, 3),
            "accel_y": round(self.accel_y_ms2, 3),
            "accel_z": round(self.accel_z_ms2, 3),
            "gyro_x":  round(self.gyro_x_rads, 4),
            "gyro_y":  round(self.gyro_y_rads, 4),
            "gyro_z":  round(self.gyro_z_rads, 4),
        }


@dataclass
class LogEntry:
    index: int
    total: int
    timestamp_ms: int
    accel_x_ms2: float
    accel_y_ms2: float
    accel_z_ms2: float
    magnitude_ms2: float

    @property
    def is_last(self) -> bool:
        return self.total == 0 or self.index == self.total - 1

    def to_dict(self) -> dict:
        return {
            "index":        self.index,
            "total":        self.total,
            "timestamp_ms": self.timestamp_ms,
            "accel_x":      round(self.accel_x_ms2, 3),
            "accel_y":      round(self.accel_y_ms2, 3),
            "accel_z":      round(self.accel_z_ms2, 3),
            "magnitude":    round(self.magnitude_ms2, 3),
        }


@dataclass
class FlashStatus:
    used_bytes: int
    total_bytes: int

    @property
    def used_pct(self) -> float:
        if self.total_bytes == 0:
            return 0.0
        return round(100.0 * self.used_bytes / self.total_bytes, 1)


# ── Decoders ─────────────────────────────────────────────────────────
def decode_shock(data: bytes) -> Optional[ShockEvent]:
    if len(data) < SIZE_SHOCK:
        print(f"[decoder] shock payload too short: {len(data)} < {SIZE_SHOCK}")
        return None
    ts, ax, ay, az, mag = struct.unpack_from(FMT_SHOCK, data)
    return ShockEvent(
        timestamp_ms=ts,
        accel_x_ms2=ax / 1000.0,
        accel_y_ms2=ay / 1000.0,
        accel_z_ms2=az / 1000.0,
        magnitude_ms2=mag / 1000.0,
    )


def decode_sensor(data: bytes) -> Optional[SensorReading]:
    if len(data) < SIZE_SENSOR:
        print(f"[decoder] sensor payload too short: {len(data)} < {SIZE_SENSOR}")
        return None
    ax, ay, az, gx, gy, gz = struct.unpack_from(FMT_SENSOR, data)
    return SensorReading(
        accel_x_ms2=ax / 1000.0,
        accel_y_ms2=ay / 1000.0,
        accel_z_ms2=az / 1000.0,
        gyro_x_rads=gx / 1000.0,
        gyro_y_rads=gy / 1000.0,
        gyro_z_rads=gz / 1000.0,
    )


def decode_log_entry(data: bytes) -> Optional[LogEntry]:
    if len(data) < SIZE_LOG_ENTRY:
        print(f"[decoder] log entry too short: {len(data)} < {SIZE_LOG_ENTRY}")
        return None
    idx, total, ts, ax, ay, az, mag = struct.unpack_from(FMT_LOG_ENTRY, data)
    return LogEntry(
        index=idx,
        total=total,
        timestamp_ms=ts,
        accel_x_ms2=ax / 100.0,   # log entries use × 100 scale
        accel_y_ms2=ay / 100.0,
        accel_z_ms2=az / 100.0,
        magnitude_ms2=mag / 100.0,
    )


def decode_flash_status(data: bytes) -> Optional[FlashStatus]:
    if len(data) < SIZE_FLASH_STATUS:
        return None
    used, total = struct.unpack_from(FMT_FLASH_STATUS, data)
    return FlashStatus(used_bytes=used, total_bytes=total)
