# BLE Asset Tracker & Shock Logger

A store-and-forward condition monitor built on the nRF52-DK (nRF52832),
using Zephyr RTOS via the nRF Connect SDK. The device logs shock/impact
events from an onboard IMU to external SPI flash during offline transit,
then syncs the full event history to a PC gateway over BLE when back in
range — the "gap-fill on reconnect" behavior is the core demo.

This is a portfolio project built from scratch to develop industry-level
embedded systems skills: devicetree configuration, peripheral driver
debugging, Zephyr's device/sensor/flash APIs, and (in later milestones)
custom BLE GATT services and a Python gateway.

## Architecture

```
 MPU-6050 (I2C)  ──┐
                    ├──>  nRF52832 firmware (Zephyr)  ──>  BLE GATT  ──>  PC gateway (Python/bleak)
 W25Q64 (SPI)    ──┘                                                          │
                                                                                v
                                                                         MQTT → InfluxDB → Grafana
```

Sensors are read by Zephyr threads, buffered through `k_msgq`, and (in a
later milestone) logged to external flash via LittleFS when shock events
are detected. A custom BLE GATT service exposes live sensor data and
historical log readback to a gateway application.

## Hardware

| Component | Interface | Notes |
|---|---|---|
| nRF52-DK (nRF52832) | — | Main MCU board |
| MPU-6050 | I2C0 | 6-axis accel + gyro |
| W25Q64JVS | SPI2 | 8MB NOR flash, external event log storage |

### Wiring

**MPU-6050 (I2C0)**
| Signal | Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | P0.27 |
| SCL | P0.26 |
| AD0 | GND (address 0x68) |
| INT | P0.02 |

**W25Q64 (SPI2)**
| Signal | Pin |
|---|---|
| VCC, WP, HOLD | 3.3V |
| GND | GND |
| MOSI | P0.23 |
| MISO | P0.24 |
| SCK | P0.25 |
| CS | P0.22 |

LEDs are remapped to P0.13–P0.16 in the devicetree overlay to avoid
conflicting with SPI-adjacent pins.

**Power note:** both sensors are wired with direct point-to-point
connections to the DK's VDD/GND pins rather than through a shared
breadboard power rail — an unreliable rail contact was a real source of
intermittent failures during bringup (see Hardware Notes below).

## Software stack

- **RTOS / SDK:** Zephyr RTOS via nRF Connect SDK v3.3.0
- **Toolchain:** nRF Connect for VS Code extension
- **Debug:** SEGGER J-Link RTT (logging), J-Link RTT Viewer

## Project structure

```
.
├── boards/
│   └── nrf52dk_nrf52832.overlay   # devicetree: pin mux, I2C/SPI config, flash partition
├── src/
│   ├── main.c                     # application entry point
│   └── flash_storage/
│       ├── flash_storage.h        # public API: init, read/write/erase, usage tracking
│       └── flash_storage.c
├── Kconfig                        # application-level config options (self-test, retries)
├── prj.conf                       # Zephyr/NCS build configuration
└── CMakeLists.txt
```

`flash_storage` wraps Zephyr's generic flash API with retry-and-verify
erase logic and a software usage tracker (used/available/total bytes).
It does not depend on `main.c` and is designed to be reusable once the
shock-event logger (Milestone 3) is built on top of it.

## Build & flash

This project is built via the nRF Connect for VS Code extension sidebar
(Build / Flash buttons), since `west` requires the toolchain environment
that extension manages — running `west build` directly in a plain
PowerShell terminal will fail with `west: command not found`.

```
Board target: nrf52dk/nrf52832
```

RTT logging is viewed via SEGGER J-Link RTT Viewer (USB, SWD, 4000kHz,
auto-detect RTT block).

### Self-test mode

`flash_storage` includes an optional self-test that runs a full
erase/write/read verification (JEDEC ID check, multiple data patterns,
usage tracking) on every boot. Controlled by:

```
CONFIG_FLASH_STORAGE_SELFTEST=y    # prj.conf
```

This is currently **enabled** during active hardware bringup. Disable it
before any extended field/production use — NOR flash has a finite
erase-cycle lifespan (~100,000 cycles per sector), and the self-test
erases its test sector on every boot.

## Status

**Milestone 1 — Hardware bringup: complete**
- MPU-6050: working, verified accel/gyro readings
- W25Q64 flash: working — erase/write/read verified across multiple
  data patterns, correct JEDEC ID and capacity reporting

**Milestone 2 — Shock detection + BLE GATT service: not started**

**Milestone 3 — LittleFS event log on flash: not started**

**Milestone 4 — OTA DFU (MCUboot/SMP): not started**

**Milestone 5 — Power optimization (coin cell, sub-5µA sleep): not started**

**Milestone 6 — Python BLE gateway + MQTT + InfluxDB + Grafana: not started**

## Hardware bringup notes

The flash subsystem went through an extended debugging process worth
documenting for anyone reading this repo:

1. **Breadboard power rail unreliability** caused intermittent failures
   on both I2C and SPI peripherals. Fixed with direct point-to-point
   wiring from DK power pins to each chip.
2. **Missing `fixed-partitions` devicetree node** caused `-EINVAL` on
   any flash access beyond the first sector.
3. **`size` property units bug** — Zephyr's `jedec,spi-nor` binding
   expects `size` in bits, not bytes; `DT_SIZE_M(8)` configured an
   8Mbit (1MB) device instead of the intended 8MB.
4. **Root cause of write corruption:** the nRF52832's default SPI
   peripheral driver (`nordic,nrf-spi`, the legacy non-DMA driver) has
   a receive-timing defect that silently corrupts multi-byte SPI
   transactions. Confirmed via logic analyzer — the physical wire
   showed correct data while the firmware read back garbage. Fixed by
   forcing `compatible = "nordic,nrf-spim"` (EasyDMA driver) on a
   dedicated SPI instance (`spi2`, which has no I2C peripheral sharing
   its underlying hardware block on the nRF52832).

## License

Personal portfolio project. No license specified yet.