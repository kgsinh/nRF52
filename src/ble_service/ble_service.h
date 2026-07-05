#ifndef BLE_SERVICE_H_
#define BLE_SERVICE_H_

#include "shock_detector/shock_detector.h"
#include <stdbool.h>

/**
 * @brief Initialize the BLE stack and start advertising.
 *
 * Registers the Asset Tracker GATT service and begins advertising
 * at 100ms interval. Must be called after all other subsystems
 * are initialized.
 *
 * @return 0 on success, negative errno on failure.
 */
int ble_service_init(void);

/**
 * @brief Push a shock event notification to a connected client.
 *
 * Called by the shock detector thread after a shock event is
 * detected. No-op if no client is connected or CCCD not enabled.
 *
 * @param event Pointer to the shock event to notify.
 */
void ble_service_notify_shock(const struct shock_event *event);

/**
 * @brief Push a live sensor notification to a connected client.
 *
 * Can be called periodically from a timer or on demand.
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