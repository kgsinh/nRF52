#include "ble_test.h"
#include "ble_service/ble_service.h"
#include "shock_detector/shock_detector.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_test, LOG_LEVEL_INF);

#define BLE_TEST_CONNECT_WAIT_MS 15000

void run_ble_test(void)
{
    LOG_INF("=== BLE Service Test ===");

    if (!ble_service_is_connected())
    {
        LOG_INF("Not connected yet — waiting %d seconds for a client...",
                BLE_TEST_CONNECT_WAIT_MS / 1000);
        LOG_INF("Scan for \"AssetTracker\" with nRF Connect app on your phone");
    }

    int waited = 0;
    while (!ble_service_is_connected() && waited < BLE_TEST_CONNECT_WAIT_MS)
    {
        k_msleep(1000);
        waited += 1000;
        LOG_INF("  Waiting... %d/%d s",
                waited / 1000, BLE_TEST_CONNECT_WAIT_MS / 1000);
    }

    if (ble_service_is_connected())
    {
        LOG_INF("BLE PASS: client connected");
        LOG_INF("  Enable notifications on the shock characteristic");
        LOG_INF("  Then bang the board to see shock events over BLE");
        k_msleep(5000); /* keep connected for manual verification */
    }
    else
    {
        LOG_WRN("BLE: no client connected within %d s",
                BLE_TEST_CONNECT_WAIT_MS / 1000);
        LOG_WRN("  Device is still advertising — connect with nRF Connect app");
    }

    LOG_INF("Total shocks since boot: %u", shock_detector_get_count());
    LOG_INF("=== BLE Service Test Complete ===");
}