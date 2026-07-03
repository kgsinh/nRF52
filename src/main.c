#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "flash_storage/flash_storage.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#if defined(CONFIG_FLASH_STORAGE_SELFTEST)

#define TEST_OFFSET 0x000000
#define TEST_SECTOR_SIZE 4096 /* W25Q64 minimum erase sector size */

static void run_flash_selftest(void)
{
	uint8_t jedec_id[3] = {0};
	flash_storage_check_id(jedec_id);

	flash_storage_print_usage("Boot");

	/* Confirm the chip is in a known (erased) state at the test offset */
	uint8_t buf[4] = {0};
	if (flash_storage_read(TEST_OFFSET, buf, sizeof(buf)) == 0)
	{
		LOG_INF("Flash bytes at offset: 0x%02X 0x%02X 0x%02X 0x%02X",
				buf[0], buf[1], buf[2], buf[3]);
	}

	/* ── Erase, then write/read pattern 1: 0xDEADBEEF ── */
	if (flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE) != 0)
	{
		LOG_ERR("Erase failed, aborting self-test");
		return;
	}
	flash_storage_print_usage("After erase");

	uint8_t wr[4] = {0xDE, 0xAD, 0xBE, 0xEF};
	if (flash_storage_write(TEST_OFFSET, wr, sizeof(wr)) == 0)
	{
		uint8_t rd[4] = {0};
		flash_storage_read(TEST_OFFSET, rd, sizeof(rd));

		if (memcmp(wr, rd, sizeof(wr)) == 0)
		{
			LOG_INF("Pattern 1 verified: 0xDEADBEEF OK");
		}
		else
		{
			LOG_ERR("Pattern 1 MISMATCH: 0x%02X%02X%02X%02X",
					rd[0], rd[1], rd[2], rd[3]);
		}
	}
	flash_storage_print_usage("After write 1 (0xDEADBEEF)");

	/* ── Erase, then write/read pattern 2: 0x12345678 ── */
	flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE);
	flash_storage_print_usage("After erase (before pattern 2)");

	uint8_t wr2[4] = {0x12, 0x34, 0x56, 0x78};
	if (flash_storage_write(TEST_OFFSET, wr2, sizeof(wr2)) == 0)
	{
		uint8_t rd2[4] = {0};
		flash_storage_read(TEST_OFFSET, rd2, sizeof(rd2));

		if (memcmp(wr2, rd2, sizeof(wr2)) == 0)
		{
			LOG_INF("Pattern 2 verified: 0x12345678 OK");
		}
		else
		{
			LOG_ERR("Pattern 2 MISMATCH: 0x%02X%02X%02X%02X",
					rd2[0], rd2[1], rd2[2], rd2[3]);
		}
	}
	flash_storage_print_usage("After write 2 (0x12345678)");

	/* ── Erase, then single-byte write/read ── */
	flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE);

	uint8_t single_byte = 0xAA;
	if (flash_storage_write(TEST_OFFSET, &single_byte, 1) == 0)
	{
		uint8_t single_read = 0x00;
		flash_storage_read(TEST_OFFSET, &single_read, 1);

		if (single_read == 0xAA)
		{
			LOG_INF("Single byte verified: 0xAA OK");
		}
		else
		{
			LOG_ERR("Single byte MISMATCH: 0x%02X", single_read);
		}
	}
	flash_storage_print_usage("After single-byte write");

	/* Clean up */
	flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE);
	flash_storage_print_usage("Final (after cleanup erase)");
}

#endif /* CONFIG_FLASH_STORAGE_SELFTEST */

int main(void)
{
	LOG_INF("=============================");
	LOG_INF("  Asset Tracker — Bringup");
	LOG_INF("=============================");
	k_msleep(200);

	if (flash_storage_init() != 0)
	{
		LOG_ERR("Flash storage init failed — halting bringup");
		while (1)
		{
			k_msleep(1000);
		}
	}

#if defined(CONFIG_FLASH_STORAGE_SELFTEST)
	LOG_INF("CONFIG_FLASH_STORAGE_SELFTEST enabled — running self-test");
	run_flash_selftest();
#else
	LOG_INF("Flash storage initialized. Self-test disabled (production mode).");
#endif

	LOG_INF("-----------------------------");
	LOG_INF("Bringup complete.");

	k_msleep(1000);
	return 0;
}