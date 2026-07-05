#include "shock_log_test.h"
#include "shock_log/shock_log.h"
#include "shock_detector/shock_detector.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(shock_log_test, LOG_LEVEL_INF);

/* Build a synthetic shock event for testing */
static struct shock_event make_test_event(int64_t ts, float ax, float ay,
                                          float az, float mag)
{
    struct shock_event e = {
        .timestamp_ms = ts,
        .magnitude = mag,
    };

    /* Pack floats into sensor_value */
    e.sample.accel[0].val1 = (int32_t)ax;
    e.sample.accel[0].val2 = (int32_t)((ax - (int32_t)ax) * 1000000);
    e.sample.accel[1].val1 = (int32_t)ay;
    e.sample.accel[1].val2 = (int32_t)((ay - (int32_t)ay) * 1000000);
    e.sample.accel[2].val1 = (int32_t)az;
    e.sample.accel[2].val2 = (int32_t)((az - (int32_t)az) * 1000000);

    return e;
}

void run_shock_log_test(void)
{
    LOG_INF("=== Shock Log Test ===");

    /* Clear any persisted real events before writing synthetic test data.
       Without this, reads 0/1/2 return old entries, not our new ones. */
    shock_log_clear();

    /* ── Write 3 synthetic events ── */
    LOG_INF("Writing 3 synthetic shock entries...");

    struct shock_event events[3] = {
        make_test_event(1000, 10.0f, 20.0f, 50.0f, 54.77f),
        make_test_event(2000, -30.0f, 5.0f, 80.0f, 85.57f),
        make_test_event(3000, 15.0f, 15.0f, 100.0f, 102.24f),
    };

    for (int i = 0; i < 3; i++)
    {
        int rc = shock_log_write(&events[i]);
        if (rc != 0)
        {
            LOG_ERR("Write %d failed: %d", i, rc);
            return;
        }
    }

    LOG_INF("Written. Count: %u", shock_log_count());

    /* ── Read back and verify ── */
    LOG_INF("Reading back entries...");
    bool all_pass = true;

    for (uint32_t i = 0; i < 3; i++)
    {
        struct shock_log_entry entry;
        int rc = shock_log_read(i, &entry);
        if (rc != 0)
        {
            LOG_ERR("Read %u failed: %d", i, rc);
            all_pass = false;
            continue;
        }

        LOG_INF("  Entry %u: ts=%lld mag=%d.%02d m/s²",
                i,
                entry.timestamp_ms,
                entry.magnitude / SHOCK_LOG_SCALE,
                entry.magnitude % SHOCK_LOG_SCALE);

        /* Verify timestamp round-trips exactly */
        if (entry.timestamp_ms != events[i].timestamp_ms)
        {
            LOG_ERR("  Timestamp mismatch: got %lld expected %lld",
                    entry.timestamp_ms, events[i].timestamp_ms);
            all_pass = false;
        }
    }

    if (all_pass)
    {
        LOG_INF("Shock log read-back PASS");
    }
    else
    {
        LOG_ERR("Shock log read-back FAIL");
    }

    /* ── Filesystem stats ── */
    uint64_t used = 0, total = 0;
    if (shock_log_fs_stats(&used, &total) == 0)
    {
        LOG_INF("LittleFS: %llu / %llu bytes used (%llu KB total)",
                used, total, total / 1024);
    }

    /* ── Cleanup ── */
    shock_log_clear();
    LOG_INF("Log cleared. Count after clear: %u", shock_log_count());

    LOG_INF("=== Shock Log Test Complete ===");
}