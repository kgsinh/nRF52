#ifndef IMU_TEST_H_
#define IMU_TEST_H_

/**
 * @brief Run the IMU bringup verification test.
 *
 * Tests: device ready, fetch, accel Z ~9.8 m/s², gyro ~0.0 rad/s.
 *
 * Only compiled in when CONFIG_FLASH_STORAGE_SELFTEST is defined
 * (reuses the same selftest gate for all bringup tests).
 */
void run_imu_test(void);

#endif /* IMU_TEST_H_ */