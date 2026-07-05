#include "shock_detector/shock_detector.h"
#include "imu/imu.h"
#include "ble_service/ble_service.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(shock_detector, LOG_LEVEL_INF);

/* ── Queue ─────────────────────────────────────────────────────────── */
K_MSGQ_DEFINE(shock_msgq,
              sizeof(struct shock_event),
              SHOCK_QUEUE_DEPTH,
              4); /* 4-byte alignment */

/* ── Module state ──────────────────────────────────────────────────── */
static float threshold_ms2 = SHOCK_THRESHOLD_DEFAULT_MS2;
static uint32_t event_count = 0;
static atomic_t initialized = ATOMIC_INIT(0);

/* ── Trigger callback (runs in GLOBAL_THREAD / work queue) ─────────── */
/* Minimum ms between fetch attempts — limits I2C load to 100Hz
   while DATA_READY interrupt fires at 1000Hz */
#define TRIGGER_MIN_INTERVAL_MS 10

static void trigger_callback(const struct device *dev,
                             const struct sensor_trigger *trigger)
{
    ARG_UNUSED(trigger);

    /* Rate-limit: skip fetch if called too soon after last attempt.
       DATA_READY fires at 1000Hz; we only need ~100Hz for shock detection. */
    static int64_t last_fetch_ms = 0;
    int64_t now = k_uptime_get();
    if ((now - last_fetch_ms) < TRIGGER_MIN_INTERVAL_MS)
    {
        return;
    }
    last_fetch_ms = now;

    /* Fetch the latest sample — we are in work queue context,
       blocking calls are allowed here */
    struct imu_sample sample;
    if (imu_fetch(&sample) != 0)
    {
        return;
    }

    /* Convert sensor_value to float for magnitude calculation */
    float ax = sensor_value_to_double(&sample.accel[0]);
    float ay = sensor_value_to_double(&sample.accel[1]);
    float az = sensor_value_to_double(&sample.accel[2]);

    float mag = sqrtf(ax * ax + ay * ay + az * az);

    /* Only post an event if magnitude exceeds threshold */
    if (mag < threshold_ms2)
    {
        return;
    }

    struct shock_event event = {
        .timestamp_ms = k_uptime_get(),
        .sample = sample,
        .magnitude = mag,
    };

    /* Non-blocking put — if queue is full, oldest event is dropped */
    if (k_msgq_put(&shock_msgq, &event, K_NO_WAIT) != 0)
    {
        LOG_WRN("Shock queue full — event dropped");
    }
}

/* ── Detector thread ───────────────────────────────────────────────── */
static void shock_detector_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Shock detector thread started (threshold: %.2f m/s²)",
            (double)threshold_ms2);

    struct shock_event event;

    while (1)
    {
        /* Block indefinitely until an event arrives */
        k_msgq_get(&shock_msgq, &event, K_FOREVER);

        event_count++;

        LOG_INF("SHOCK #%u detected at %lld ms",
                event_count, event.timestamp_ms);
        LOG_INF("  Magnitude: %.2f m/s² (threshold: %.2f)",
                (double)event.magnitude, (double)threshold_ms2);
        LOG_INF("  Accel  X: %d.%06d  Y: %d.%06d  Z: %d.%06d",
                event.sample.accel[0].val1,
                abs(event.sample.accel[0].val2),
                event.sample.accel[1].val1,
                abs(event.sample.accel[1].val2),
                event.sample.accel[2].val1,
                abs(event.sample.accel[2].val2));

        /* Notify BLE client if connected */
        ble_service_notify_shock(&event);

        /* Milestone 3: write event to flash ring buffer here */
        /* Milestone 4: notify BLE characteristic here       */
    }
}

/* Stack and thread definition — compiled in at link time */
K_THREAD_DEFINE(shock_thread,
                1024, /* stack size in bytes */
                shock_detector_thread,
                NULL, NULL, NULL,
                7,  /* priority (lower = higher) */
                0,  /* options */
                0); /* delay before start (ms) */

/* ── Public API ────────────────────────────────────────────────────── */
int shock_detector_init(float threshold)
{
    if (!imu_is_ready())
    {
        LOG_ERR("IMU not ready — call imu_init() before shock_detector_init()");
        return -ENODEV;
    }

    threshold_ms2 = threshold;

    /* Arm the MPU-6050 data-ready trigger */
    static const struct device *imu_dev =
        DEVICE_DT_GET_ANY(invensense_mpu6050);

    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    int rc = sensor_trigger_set(imu_dev, &trig, trigger_callback);
    if (rc != 0)
    {
        LOG_ERR("sensor_trigger_set failed: %d", rc);
        return rc;
    }

    atomic_set(&initialized, 1);
    LOG_INF("Shock detector armed (threshold: %.2f m/s²)",
            (double)threshold_ms2);

    return 0;
}

void shock_detector_set_threshold(float threshold)
{
    threshold_ms2 = threshold;
    LOG_INF("Shock threshold updated: %.2f m/s²", (double)threshold_ms2);
}

uint32_t shock_detector_get_count(void)
{
    return event_count;
}