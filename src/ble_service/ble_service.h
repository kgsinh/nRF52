#ifndef BLE_SERVICE_H_
#define BLE_SERVICE_H_

#include "shock_detector/shock_detector.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief One log entry as transmitted over BLE — 20 bytes.
 *
 * Fits within the default 23-byte ATT MTU. The client uses
 * index/total to track stream progress and detect completion.
 * Values are fixed-point (× 100) to avoid float on the gateway.
 */
struct __packed ble_log_entry_payload
{
    uint16_t index;       /* entry index (0-based)           */
    uint16_t total;       /* total entries in log            */
    int64_t timestamp_ms; /* k_uptime_get() at shock time    */
    int16_t accel_x;      /* m/s² × 100                      */
    int16_t accel_y;
    int16_t accel_z;
    uint16_t magnitude; /* m/s² × 100                      */
}; /* 20 bytes total                  */

/**
 * @brief Initialize the BLE stack and start advertising.
 *
 * @return 0 on success, negative errno on failure.
 */
int ble_service_init(void);

/**
 * @brief Push a shock event notification to a connected client.
 *
 * No-op if no client is connected or CCCD not enabled.
 *
 * @param event Pointer to the shock event to notify.
 */
void ble_service_notify_shock(const struct shock_event *event);

/**
 * @brief Push a live sensor notification to a connected client.
 *
 * @param sample Pointer to the IMU sample to notify.
 */
void ble_service_notify_sensor(const struct imu_sample *sample);

/**
 * @brief Check if a client is currently connected.
 *
 * @return true if connected, false otherwise.
 */
bool ble_service_is_connected(void);

#endif /* BLE_SERVICE_H_ */