#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "flash_storage/flash_storage.h"
#include "imu/imu.h"
#include "shock_detector/shock_detector.h"
#include "shock_log/shock_log.h"
#include "ble_service/ble_service.h"

#if defined(CONFIG_BRINGUP_SELFTEST)
#include "flash_test.h"
#include "imu_test.h"
#include "shock_test.h"
#include "shock_log_test.h"
#include "ble_test.h"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("=============================");
	LOG_INF("  Asset Tracker — Bringup");
	LOG_INF("=============================");
	k_msleep(200);

	/* ── Initialize subsystems in dependency order ── */
	imu_init();

	if (flash_storage_init() != 0)
	{
		LOG_ERR("Flash storage init failed — halting");
		while (1)
		{
			k_msleep(1000);
		}
	}

	if (shock_log_init() != 0)
	{
		LOG_ERR("Shock log init failed — halting");
		while (1)
		{
			k_msleep(1000);
		}
	}

	if (shock_detector_init(SHOCK_THRESHOLD_DEFAULT_MS2) != 0)
	{
		LOG_ERR("Shock detector init failed — halting");
		while (1)
		{
			k_msleep(1000);
		}
	}

	if (ble_service_init() != 0)
	{
		LOG_ERR("BLE service init failed — halting");
		while (1)
		{
			k_msleep(1000);
		}
	}

	/* ── Bringup self-tests (compiled out in production) ── */
#if defined(CONFIG_BRINGUP_SELFTEST)
	LOG_INF("--- Bringup Self-Tests ---");
	run_imu_test();
	run_flash_selftest();
	run_shock_log_test();
	run_shock_test();
	run_ble_test();
#endif

	LOG_INF("-----------------------------");
	LOG_INF("Bringup complete.");

	k_msleep(1000);
	return 0;
}