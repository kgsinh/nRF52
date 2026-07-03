#include "imu_test.h"
#include "imu/imu.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu_test, LOG_LEVEL_INF);

void run_imu_test(void)
{
    LOG_INF("=== IMU Test ===");

    if (!imu_is_ready())
    {
        LOG_ERR("IMU not initialized — call imu_init() before run_imu_test()");
        return;
    }

    struct imu_sample sample;
    int rc = imu_fetch(&sample);
    if (rc != 0)
    {
        LOG_ERR("IMU fetch failed: %d", rc);
        return;
    }

    imu_log_sample(&sample);

    /* Sanity check: Z accel should be ~9.8 m/s² if board is flat */
    int z = sample.accel[2].val1;
    if (z >= 8 && z <= 11)
    {
        LOG_INF("IMU PASS: Z accel %d m/s² (expected ~9.8)", z);
    }
    else
    {
        LOG_WRN("IMU WARN: Z accel %d m/s² — is the board flat?", z);
    }

    LOG_INF("=== IMU Test Complete ===");
}