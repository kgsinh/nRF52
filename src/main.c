#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(milestone1, LOG_LEVEL_DBG);

#define TEST_OFFSET 0x000000

/* Get flash device from device tree */
static const struct device *flash = DEVICE_DT_GET_ANY(jedec_spi_nor);

/* ── Track used/occupied space ── */
static uint64_t flash_total_size = 0;
static uint64_t flash_bytes_used = 0;

static void print_flash_usage(const char *label)
{
	uint64_t available = (flash_total_size > flash_bytes_used)
							 ? (flash_total_size - flash_bytes_used)
							 : 0;
	LOG_INF("[%s] Used: %llu bytes | Available: %llu bytes | Total: %llu bytes",
			label, flash_bytes_used, available, flash_total_size);
}

/* ── Check W25Q64 ── */
static void check_w25q64(void)
{
	if (!device_is_ready(flash))
	{
		LOG_ERR("W25Q64 not ready");
		LOG_ERR("  Check: MOSI→P0.23, MISO→P0.24, SCK→P0.25, CS→P0.22");
		LOG_ERR("  Check: WP and HOLD tied to 3.3V");
		return;
	}
	LOG_INF("W25Q64 flash device ready OK");
	LOG_INF("Using TEST_OFFSET = 0x%06X", TEST_OFFSET);

	/* ── Read JEDEC manufacturer/device ID ── */
	uint8_t jedec_id[3] = {0};
	int rc = flash_read_jedec_id(flash, jedec_id);
	if (rc == 0)
	{
		LOG_INF("JEDEC ID: %02X %02X %02X (manufacturer %02X)",
				jedec_id[0], jedec_id[1], jedec_id[2], jedec_id[0]);
	}
	else
	{
		LOG_WRN("flash_read_jedec_id failed: %d", rc);
	}

	/* ── Read total flash size ── */
	rc = flash_get_size(flash, &flash_total_size);
	if (rc == 0)
	{
		LOG_INF("Flash total size: %llu bytes (%llu MB)",
				flash_total_size, flash_total_size / (1024 * 1024));
	}
	else
	{
		LOG_WRN("flash_get_size failed: %d", rc);
	}
	print_flash_usage("Boot");

	/* Read first 4 bytes at the test offset */
	uint8_t buf[4] = {0};
	rc = flash_read(flash, TEST_OFFSET, buf, sizeof(buf));
	if (rc != 0)
	{
		LOG_ERR("Flash read failed: %d", rc);
		return;
	}
	LOG_INF("Flash bytes at offset: 0x%02X 0x%02X 0x%02X 0x%02X",
			buf[0], buf[1], buf[2], buf[3]);
	LOG_INF("  (0xFF = blank chip — that is correct)");

	/* Get flash parameters to check minimum erase size */
	const struct flash_parameters *params = flash_get_parameters(flash);
	if (params != NULL)
	{
		LOG_INF("Flash write block size: %u bytes", params->write_block_size);
		LOG_INF("Flash erase value: 0x%02X", params->erase_value);
	}

	struct flash_pages_info page_info;
	rc = flash_get_page_info_by_offs(flash, TEST_OFFSET, &page_info);
	if (rc == 0)
	{
		LOG_INF("Flash page/sector size at test offset: %u bytes", page_info.size);
	}
	else
	{
		LOG_WRN("Could not get page info: %d", rc);
	}

	/* ── Erase with retry loop — 4KB sector erase, forced wait, up to 3 attempts ── */
	LOG_INF("Testing write/read/erase cycle at offset 0x%06X...", TEST_OFFSET);

	bool erase_ok = false;
	uint8_t check[4] = {0};

	for (int attempt = 1; attempt <= 3; attempt++)
	{
		int64_t t_start = k_uptime_get();
		rc = flash_erase(flash, TEST_OFFSET, 4096);
		int64_t t_end = k_uptime_get();
		LOG_INF("Erase attempt %d returned: %d (took %lld ms before forced wait)",
				attempt, rc, t_end - t_start);

		LOG_INF("  Forcing extra 500ms wait...");
		k_msleep(500);

		if (rc == 0)
		{
			memset(check, 0xA5, sizeof(check));
			rc = flash_read(flash, TEST_OFFSET, check, sizeof(check));
			LOG_INF("  Read after attempt %d returned: %d", attempt, rc);
			LOG_INF("  Bytes after attempt %d: 0x%02X 0x%02X 0x%02X 0x%02X",
					attempt, check[0], check[1], check[2], check[3]);

			if (rc != 0)
			{
				continue;
			}

			if (check[0] == 0xFF && check[1] == 0xFF &&
				check[2] == 0xFF && check[3] == 0xFF)
			{
				LOG_INF("  SUCCESS on attempt %d", attempt);
				erase_ok = true;
				/* Erase clears the 4KB sector — reduce used count accordingly */
				if (flash_bytes_used >= 4096)
				{
					flash_bytes_used -= 4096;
				}
				else
				{
					flash_bytes_used = 0;
				}
				break;
			}
		}
		k_msleep(100);
	}

	if (!erase_ok)
	{
		LOG_ERR("Erase did NOT produce 0xFF after 3 attempts (even with 500ms forced wait)");
		LOG_ERR("  This points to write-protect, or erase command never reaching chip");
		return;
	}
	print_flash_usage("After erase");

	/* ── Write test — pattern 1: 0xDEADBEEF ── */
	uint8_t wr[4] = {0xDE, 0xAD, 0xBE, 0xEF};
	rc = flash_write(flash, TEST_OFFSET, wr, sizeof(wr));
	LOG_INF("Write returned: %d", rc);
	if (rc != 0)
	{
		LOG_ERR("Write failed: %d", rc);
		return;
	}
	flash_bytes_used += sizeof(wr);

	k_msleep(50);

	uint8_t rd[4] = {0};
	rc = flash_read(flash, TEST_OFFSET, rd, sizeof(rd));
	LOG_INF("Read returned: %d", rc);

	if (rd[0] == 0xDE && rd[1] == 0xAD && rd[2] == 0xBE && rd[3] == 0xEF)
	{
		LOG_INF("Flash write/read verified: 0xDEADBEEF OK");
	}
	else
	{
		LOG_ERR("Flash readback MISMATCH: 0x%02X%02X%02X%02X",
				rd[0], rd[1], rd[2], rd[3]);
	}
	print_flash_usage("After write 1 (0xDEADBEEF)");

	/* ── Write test — pattern 2: 0x12345678 ── */
	LOG_INF("Running second write/read test with different pattern...");
	rc = flash_erase(flash, TEST_OFFSET, 4096);
	LOG_INF("Erase before pattern 2 returned: %d", rc);
	if (rc == 0)
	{
		flash_bytes_used = (flash_bytes_used >= 4096) ? (flash_bytes_used - 4096) : 0;
	}
	k_msleep(500);
	print_flash_usage("After erase (before pattern 2)");

	uint8_t wr2[4] = {0x12, 0x34, 0x56, 0x78};
	rc = flash_write(flash, TEST_OFFSET, wr2, sizeof(wr2));
	LOG_INF("Write 2 returned: %d", rc);
	if (rc == 0)
	{
		flash_bytes_used += sizeof(wr2);
	}
	k_msleep(50);

	uint8_t rd2[4] = {0};
	rc = flash_read(flash, TEST_OFFSET, rd2, sizeof(rd2));
	LOG_INF("Read 2 returned: %d", rc);
	LOG_INF("Pattern 2 readback: 0x%02X%02X%02X%02X", rd2[0], rd2[1], rd2[2], rd2[3]);

	if (rd2[0] == 0x12 && rd2[1] == 0x34 && rd2[2] == 0x56 && rd2[3] == 0x78)
	{
		LOG_INF("Pattern 2 verified: 0x12345678 OK");
	}
	else
	{
		LOG_ERR("Pattern 2 MISMATCH — expected 0x12345678, got 0x%02X%02X%02X%02X",
				rd2[0], rd2[1], rd2[2], rd2[3]);
	}
	print_flash_usage("After write 2 (0x12345678)");

	/* ── Single byte write test ── */
	LOG_INF("Running single-byte write test...");
	rc = flash_erase(flash, TEST_OFFSET, 4096);
	LOG_INF("Erase before single-byte test returned: %d", rc);
	if (rc == 0)
	{
		flash_bytes_used = (flash_bytes_used >= 4096) ? (flash_bytes_used - 4096) : 0;
	}
	k_msleep(500);

	uint8_t single_byte = 0xAA;
	rc = flash_write(flash, TEST_OFFSET, &single_byte, 1);
	LOG_INF("Single byte write returned: %d", rc);
	if (rc == 0)
	{
		flash_bytes_used += 1;
	}
	k_msleep(50);

	uint8_t single_read = 0x00;
	rc = flash_read(flash, TEST_OFFSET, &single_read, 1);
	LOG_INF("Single byte read returned: %d, value: 0x%02X (expected 0xAA)", rc, single_read);

	if (single_read == 0xAA)
	{
		LOG_INF("Single byte write/read verified OK");
	}
	else
	{
		LOG_ERR("Single byte MISMATCH — expected 0xAA, got 0x%02X", single_read);
	}
	print_flash_usage("After single-byte write");

	/* Clean up */
	rc = flash_erase(flash, TEST_OFFSET, 4096);
	if (rc == 0)
	{
		flash_bytes_used = (flash_bytes_used >= 4096) ? (flash_bytes_used - 4096) : 0;
	}
	k_msleep(500);
	LOG_INF("Flash test page erased OK");
	print_flash_usage("Final (after cleanup erase)");
}

/* ── Main ── */
int main(void)
{
	LOG_INF("=============================");
	LOG_INF("  Flash-only Test — W25Q64");
	LOG_INF("=============================");
	k_msleep(200);

	check_w25q64();

	LOG_INF("-----------------------------");
	LOG_INF("Done.");

	k_msleep(1000);
	return 0;
}