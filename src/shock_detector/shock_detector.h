#ifndef SHOCK_DETECTOR_H_
#define SHOCK_DETECTOR_H_

#include "imu/imu.h"
#include <stdint.h>

/** Default shock threshold — 5g in m/s²
 *  Filters normal handling (picking up, walking, vehicle vibration).
 *  Triggers on genuine rough handling: drops, impacts, throws.
 *  At ±16g accel range the sensor can accurately measure up to ~157 m/s². */
#define SHOCK_THRESHOLD_DEFAULT_MS2 49.05f

/** Maximum shock events queued before oldest is dropped */
#define SHOCK_QUEUE_DEPTH 8

/**
 * @brief A single detected shock event.
 *
 * Populated by the trigger callback and consumed by the
 * shock_detector thread. Designed to be serialized directly
 * to flash in Milestone 3.
 */
struct shock_event
{
    int64_t timestamp_ms;     /* k_uptime_get() at detection */
    struct imu_sample sample; /* full accel + gyro at moment of shock */
    float magnitude;          /* sqrt(ax²+ay²+az²) in m/s² */
};

/**
 * @brief Initialize the shock detector.
 *
 * Arms the MPU-6050 data-ready trigger, starts the detector
 * thread, and sets the initial threshold.
 *
 * @param threshold_ms2 Shock threshold in m/s².
 *                      Use SHOCK_THRESHOLD_DEFAULT_MS2 for 2g.
 * @return 0 on success, negative errno on failure.
 */
int shock_detector_init(float threshold_ms2);

/**
 * @brief Update the shock detection threshold at runtime.
 *
 * @param threshold_ms2 New threshold in m/s².
 */
void shock_detector_set_threshold(float threshold_ms2);

/**
 * @brief Get the total number of shock events detected since boot.
 *
 * @return Event count.
 */
uint32_t shock_detector_get_count(void);

#endif /* SHOCK_DETECTOR_H_ */