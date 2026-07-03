#ifndef IMU_H_
#define IMU_H_

#include <zephyr/drivers/sensor.h>
#include <stdbool.h>

/**
 * @brief IMU sample — raw sensor_value fields from one fetch.
 *
 * val1 is the integer part, val2 is the fractional part in
 * millionths (micro-units). To reconstruct: value = val1 + val2 * 1e-6
 */
struct imu_sample
{
    struct sensor_value accel[3]; /* m/s² — X, Y, Z */
    struct sensor_value gyro[3];  /* rad/s — X, Y, Z */
};

/**
 * @brief Initialize the IMU module.
 *
 * Confirms the MPU-6050 device is ready. Must be called once
 * before any other imu_* function.
 *
 * @return 0 on success, negative errno on failure.
 */
int imu_init(void);

/**
 * @brief Fetch a fresh sample from the IMU.
 *
 * Triggers a sensor_sample_fetch() + sensor_channel_get() and
 * populates the provided sample struct.
 *
 * @param sample Output struct to fill with accel and gyro data.
 * @return 0 on success, negative errno on failure.
 */
int imu_fetch(struct imu_sample *sample);

/**
 * @brief Log a human-readable IMU sample to the RTT console.
 *
 * @param sample Pointer to a previously filled imu_sample.
 */
void imu_log_sample(const struct imu_sample *sample);

/**
 * @brief Check if the IMU device is ready.
 *
 * @return true if initialized and ready, false otherwise.
 */
bool imu_is_ready(void);

#endif /* IMU_H_ */