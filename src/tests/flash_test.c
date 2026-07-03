#include "flash_test.h"
#include "flash_storage/flash_storage.h"

#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(flash_test, LOG_LEVEL_INF);

#define TEST_OFFSET 0x000000
#define TEST_SECTOR_SIZE 4096

void run_flash_selftest(void)
{
    LOG_INF("=== Flash Self-Test ===");

    /* ── JEDEC ID + size ── */
    uint8_t jedec_id[3] = {0};
    flash_storage_check_id(jedec_id);
    flash_storage_print_usage("Boot");

    /* ── Initial read (confirm blank or known state) ── */
    uint8_t buf[4] = {0};
    if (flash_storage_read(TEST_OFFSET, buf, sizeof(buf)) == 0)
    {
        LOG_INF("Bytes at offset: 0x%02X 0x%02X 0x%02X 0x%02X",
                buf[0], buf[1], buf[2], buf[3]);
    }

    /* ── Erase ── */
    if (flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE) != 0)
    {
        LOG_ERR("Erase failed — aborting");
        return;
    }
    flash_storage_print_usage("After erase");

    /* ── Pattern 1: 0xDEADBEEF ── */
    uint8_t wr1[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    if (flash_storage_write(TEST_OFFSET, wr1, sizeof(wr1)) == 0)
    {
        uint8_t rd1[4] = {0};
        flash_storage_read(TEST_OFFSET, rd1, sizeof(rd1));
        if (memcmp(wr1, rd1, sizeof(wr1)) == 0)
        {
            LOG_INF("Pattern 1 PASS: 0xDEADBEEF");
        }
        else
        {
            LOG_ERR("Pattern 1 FAIL: got 0x%02X%02X%02X%02X",
                    rd1[0], rd1[1], rd1[2], rd1[3]);
        }
    }
    flash_storage_print_usage("After write 1");

    /* ── Pattern 2: 0x12345678 ── */
    flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE);
    flash_storage_print_usage("After erase (before pattern 2)");

    uint8_t wr2[4] = {0x12, 0x34, 0x56, 0x78};
    if (flash_storage_write(TEST_OFFSET, wr2, sizeof(wr2)) == 0)
    {
        uint8_t rd2[4] = {0};
        flash_storage_read(TEST_OFFSET, rd2, sizeof(rd2));
        if (memcmp(wr2, rd2, sizeof(wr2)) == 0)
        {
            LOG_INF("Pattern 2 PASS: 0x12345678");
        }
        else
        {
            LOG_ERR("Pattern 2 FAIL: got 0x%02X%02X%02X%02X",
                    rd2[0], rd2[1], rd2[2], rd2[3]);
        }
    }
    flash_storage_print_usage("After write 2");

    /* ── Single byte: 0xAA ── */
    flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE);

    uint8_t wb = 0xAA;
    if (flash_storage_write(TEST_OFFSET, &wb, 1) == 0)
    {
        uint8_t rb = 0x00;
        flash_storage_read(TEST_OFFSET, &rb, 1);
        if (rb == 0xAA)
        {
            LOG_INF("Single byte PASS: 0xAA");
        }
        else
        {
            LOG_ERR("Single byte FAIL: got 0x%02X", rb);
        }
    }
    flash_storage_print_usage("After single-byte write");

    /* ── Cleanup ── */
    flash_storage_erase(TEST_OFFSET, TEST_SECTOR_SIZE);
    flash_storage_print_usage("Final (cleanup done)");

    LOG_INF("=== Flash Self-Test Complete ===");
}