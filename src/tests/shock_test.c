#include "shock_test.h"
#include "shock_detector/shock_detector.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shock_test, LOG_LEVEL_INF);

#define SHOCK_TEST_WAIT_MS 10000 /* 10 seconds — enough time to react */
#define SHOCK_TEST_COUNTDOWN 3   /* seconds of countdown before window opens */

void run_shock_test(void)
{
    LOG_INF("=== Shock Detector Test ===");
    LOG_INF("Threshold: %.2f m/s² (5g)", (double)SHOCK_THRESHOLD_DEFAULT_MS2);

    /* Countdown so user has time to get ready */
    LOG_INF("Get ready to shake the board...");
    for (int i = SHOCK_TEST_COUNTDOWN; i > 0; i--)
    {
        LOG_INF("  Starting in %d...", i);
        k_msleep(1000);
    }

    LOG_INF(">>> SHAKE THE BOARD NOW — %d seconds <<<",
            SHOCK_TEST_WAIT_MS / 1000);

    uint32_t count_before = shock_detector_get_count();

    k_msleep(SHOCK_TEST_WAIT_MS);

    uint32_t count_after = shock_detector_get_count();
    uint32_t detected = count_after - count_before;

    if (detected > 0)
    {
        LOG_INF("Shock test PASS: %u event(s) detected", detected);
    }
    else
    {
        LOG_WRN("Shock test: 0 events — board was not shaken, "
                "or threshold too high");
    }

    LOG_INF("Total shock events since boot: %u", count_after);
    LOG_INF("=== Shock Detector Test Complete ===");
}