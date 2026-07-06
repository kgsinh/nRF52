"""
ble_client.py — BLE client for AssetTracker.

Scans for the device by name, connects, enables notifications on all
characteristics, triggers log readback on connect, and reconnects
automatically on disconnect.
"""

import asyncio
import struct
import traceback
from bleak import BleakScanner, BleakClient
from decoder import (
    decode_shock, decode_sensor, decode_log_entry, decode_flash_status
)

# ── UUIDs — must match ble_service.c exactly ─────────────────────────
UUID_SERVICE      = "12345678-1234-5678-1234-56789abcdef0"
UUID_SENSOR       = "12345678-1234-5678-1234-56789abcdef1"
UUID_SHOCK        = "12345678-1234-5678-1234-56789abcdef2"
UUID_CONFIG       = "12345678-1234-5678-1234-56789abcdef3"
UUID_FLASH_STATUS = "12345678-1234-5678-1234-56789abcdef4"
UUID_LOG_READBACK = "12345678-1234-5678-1234-56789abcdef5"

DEVICE_NAME     = "AssetTracker"
SCAN_TIMEOUT_S  = 10
RECONNECT_DELAY = 5
READ_FLASH_STATUS = False


class AssetTrackerClient:
    def __init__(self, callbacks: dict):
        """
        callbacks keys:
            on_shock(ShockEvent)
            on_sensor(SensorReading)
            on_log_entry(LogEntry)
            on_status(dict)
        """
        self._callbacks = callbacks
        self._client    = None
        self._running   = False

    async def run_forever(self):
        """Scan, connect, and reconnect automatically until stopped."""
        self._running = True
        while self._running:
            try:
                await self._connect_and_run()
            except Exception as e:
                print(f"[ble] Error: {e}")
                traceback.print_exc()

            if self._running:
                print(f"[ble] Reconnecting in {RECONNECT_DELAY}s...")
                await asyncio.sleep(RECONNECT_DELAY)

    def stop(self):
        self._running = False

    async def _connect_and_run(self):
        """Scan for the device, connect, enable notifications, run."""
        print(f"[ble] Scanning for '{DEVICE_NAME}'...")
        device = await BleakScanner.find_device_by_name(
            DEVICE_NAME, timeout=SCAN_TIMEOUT_S
        )

        if device is None:
            print(f"[ble] '{DEVICE_NAME}' not found — will retry")
            return

        print(f"[ble] Found {device.name} at {device.address}")

        async with BleakClient(device) as client:
            self._client = client
            print(f"[ble] Connected to {device.address}")

            self._notify_status({"event": "connected",
                                  "address": device.address})

            # Enable notifications
            print("[ble] Enabling sensor notifications...")
            await client.start_notify(UUID_SENSOR,
                                       self._handle_sensor)
            print("[ble] Sensor notifications enabled")
            print("[ble] Enabling shock notifications...")
            await client.start_notify(UUID_SHOCK,
                                       self._handle_shock)
            print("[ble] Shock notifications enabled")
            print("[ble] Enabling log readback notifications...")
            await client.start_notify(UUID_LOG_READBACK,
                                       self._handle_log_entry)
            print("[ble] Log readback notifications enabled")

            # Read flash status
            if READ_FLASH_STATUS:
                print("[ble] Reading flash status...")
                try:
                    raw = await client.read_gatt_char(UUID_FLASH_STATUS,
                                                      use_cached=False)
                    status = decode_flash_status(bytes(raw))
                    if status:
                        print(f"[ble] Flash: {status.used_bytes}B used "
                              f"/ {status.total_bytes}B total "
                              f"({status.used_pct}%)")
                        self._notify_status({
                            "event":       "flash_status",
                            "used_bytes":  status.used_bytes,
                            "total_bytes": status.total_bytes,
                            "used_pct":    status.used_pct,
                        })

            # Trigger log readback — write 0x01
                except OSError as e:
                    print(f"[ble] Flash status read failed: {e} "
                          f"- continuing without flash status")

            else:
                print("[ble] Flash status read disabled")

            print("[ble] Requesting log readback...")
            if not client.is_connected:
                print("[ble] Disconnected before log readback request")
                return
            await client.write_gatt_char(
                UUID_LOG_READBACK, bytes([0x01]), response=True
            )

            # Keep running until disconnection
            print("[ble] Listening for notifications — Ctrl+C to stop")
            while client.is_connected and self._running:
                await asyncio.sleep(1.0)

            self._notify_status({"event": "disconnected"})
            self._client = None
            print("[ble] Disconnected")

    # ── Notification handlers ────────────────────────────────────────
    def _handle_shock(self, sender, data: bytearray):
        event = decode_shock(bytes(data))
        if event:
            print(f"[ble] SHOCK: mag={event.magnitude_ms2:.1f} m/s² "
                  f"ts={event.timestamp_ms}ms")
            cb = self._callbacks.get("on_shock")
            if cb:
                cb(event)

    def _handle_sensor(self, sender, data: bytearray):
        reading = decode_sensor(bytes(data))
        if reading:
            cb = self._callbacks.get("on_sensor")
            if cb:
                cb(reading)

    def _handle_log_entry(self, sender, data: bytearray):
        entry = decode_log_entry(bytes(data))
        if entry:
            print(f"[ble] LOG [{entry.index+1}/{entry.total}] "
                  f"ts={entry.timestamp_ms}ms "
                  f"mag={entry.magnitude_ms2:.1f} m/s²")
            cb = self._callbacks.get("on_log_entry")
            if cb:
                cb(entry)
            if entry.is_last:
                print(f"[ble] Log readback complete ({entry.total} entries)")

    def _notify_status(self, data: dict):
        cb = self._callbacks.get("on_status")
        if cb:
            cb(data)
