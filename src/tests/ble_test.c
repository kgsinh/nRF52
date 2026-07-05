#include "ble_test.h"
#include "ble_service/ble_service.h"
#include "shock_log/shock_log.h"
#include "shock_detector/shock_detector.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_test, LOG_LEVEL_INF);

#define BLE_TEST_CONNECT_WAIT_MS 15000

void run_ble_test(void)
{
    LOG_INF("=== BLE Service Test ===");

    /* ── Wait for client connection ── */
    if (!ble_service_is_connected())
    {
        LOG_INF("Waiting %d seconds for a client...",
                BLE_TEST_CONNECT_WAIT_MS / 1000);
        LOG_INF("Scan for \"AssetTracker\" with nRF Connect app");
    }

    int waited = 0;
    while (!ble_service_is_connected() && waited < BLE_TEST_CONNECT_WAIT_MS)
    {
        k_msleep(1000);
        waited += 1000;
        LOG_INF("  Waiting... %d/%d s",
                waited / 1000, BLE_TEST_CONNECT_WAIT_MS / 1000);
    }

    if (!ble_service_is_connected())
    {
        LOG_WRN("BLE: no client connected within %d s",
                BLE_TEST_CONNECT_WAIT_MS / 1000);
        LOG_WRN("  Device is still advertising");
        LOG_INF("=== BLE Service Test Complete ===");
        return;
    }

    LOG_INF("BLE PASS: client connected");

    /* ── Log readback test ── */
    uint32_t stored = shock_log_count();
    LOG_INF("Log readback test: %u entries stored", stored);

    if (stored == 0)
    {
        LOG_WRN("No stored events to read back — bang the board first");
        LOG_WRN("  Reboot after creating some shock events to test readback");
    }
    else
    {
        LOG_INF("To trigger readback: write 0x01 to the Log Readback");
        LOG_INF("characteristic (UUID ending ...def5) in nRF Connect");
        LOG_INF("Enable notifications on that characteristic first,");
        LOG_INF("then you will receive %u entries as notifications.", stored);

        /* Brief wait so user can set up notifications manually */
        k_msleep(5000);
        LOG_INF("Readback entries will stream after client enables");
        LOG_INF("notifications and writes 0x01 to trigger.");
    }

    LOG_INF("Total shocks since boot: %u", shock_detector_get_count());
    LOG_INF("=== BLE Service Test Complete ===");
}