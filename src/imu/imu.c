#include "imu.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu, LOG_LEVEL_INF);

static const struct device *imu_dev = DEVICE_DT_GET_ANY(invensense_mpu6050);
static bool initialized = false;

int imu_init(void)
{
    if (!device_is_ready(imu_dev))
    {
        LOG_ERR("MPU-6050 not ready");
        LOG_ERR("  Check: SDA→P0.27, SCL→P0.26, VCC→3.3V, GND, AD0→GND");
        return -ENODEV;
    }

    initialized = true;
    LOG_INF("MPU-6050 ready OK");
    return 0;
}

bool imu_is_ready(void)
{
    return initialized;
}

int imu_fetch(struct imu_sample *sample)
{
    if (!initialized)
    {
        LOG_ERR("IMU not initialized — call imu_init() first");
        return -ENODEV;
    }

    if (sample == NULL)
    {
        return -EINVAL;
    }

    int rc = sensor_sample_fetch(imu_dev);
    if (rc != 0)
    {
        LOG_ERR("sensor_sample_fetch failed: %d", rc);
        return rc;
    }

    rc = sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, sample->accel);
    if (rc != 0)
    {
        LOG_ERR("accel channel_get failed: %d", rc);
        return rc;
    }

    rc = sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, sample->gyro);
    if (rc != 0)
    {
        LOG_ERR("gyro channel_get failed: %d", rc);
        return rc;
    }

    return 0;
}

void imu_log_sample(const struct imu_sample *sample)
{
    if (sample == NULL)
    {
        return;
    }

    LOG_INF("--- Accelerometer (m/s2) ---");
    LOG_INF("  X: %d.%06d", sample->accel[0].val1, abs(sample->accel[0].val2));
    LOG_INF("  Y: %d.%06d", sample->accel[1].val1, abs(sample->accel[1].val2));
    LOG_INF("  Z: %d.%06d  <-- expect ~9.8 if flat",
            sample->accel[2].val1, abs(sample->accel[2].val2));

    LOG_INF("--- Gyroscope (rad/s) ---");
    LOG_INF("  X: %d.%06d", sample->gyro[0].val1, abs(sample->gyro[0].val2));
    LOG_INF("  Y: %d.%06d", sample->gyro[1].val1, abs(sample->gyro[1].val2));
    LOG_INF("  Z: %d.%06d  <-- expect ~0.0 if still",
            sample->gyro[2].val1, abs(sample->gyro[2].val2));
}