#include "flash_storage.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(flash_storage, LOG_LEVEL_INF);

static const struct device *flash_dev = DEVICE_DT_GET_ANY(jedec_spi_nor);

static uint64_t total_size = 0;
static uint64_t bytes_used = 0;

int flash_storage_init(void)
{
    if (!device_is_ready(flash_dev))
    {
        LOG_ERR("Flash device not ready");
        LOG_ERR("  Check: MOSI, MISO, SCK, CS wiring and power");
        return -ENODEV;
    }

    int rc = flash_get_size(flash_dev, &total_size);
    if (rc != 0)
    {
        LOG_WRN("flash_get_size failed: %d", rc);
        total_size = 0;
    }

    bytes_used = 0;

    LOG_INF("Flash storage ready. Total size: %llu bytes (%llu MB)",
            total_size, total_size / (1024 * 1024));

    return 0;
}

int flash_storage_check_id(uint8_t id[3])
{
    if (!device_is_ready(flash_dev))
    {
        return -ENODEV;
    }

    int rc = flash_read_jedec_id(flash_dev, id);
    if (rc == 0)
    {
        LOG_INF("JEDEC ID: %02X %02X %02X (manufacturer %02X)",
                id[0], id[1], id[2], id[0]);
    }
    else
    {
        LOG_WRN("flash_read_jedec_id failed: %d", rc);
    }

    return rc;
}

uint64_t flash_storage_get_total_size(void)
{
    return total_size;
}

uint64_t flash_storage_get_used(void)
{
    return bytes_used;
}

uint64_t flash_storage_get_available(void)
{
    return (total_size > bytes_used) ? (total_size - bytes_used) : 0;
}

void flash_storage_print_usage(const char *label)
{
    LOG_INF("[%s] Used: %llu bytes | Available: %llu bytes | Total: %llu bytes",
            label, flash_storage_get_used(), flash_storage_get_available(), total_size);
}

int flash_storage_read(uint32_t offset, uint8_t *data, size_t len)
{
    if (!device_is_ready(flash_dev))
    {
        return -ENODEV;
    }

    int rc = flash_read(flash_dev, offset, data, len);
    if (rc != 0)
    {
        LOG_ERR("flash_read failed at 0x%06X: %d", offset, rc);
    }

    return rc;
}

int flash_storage_write(uint32_t offset, const uint8_t *data, size_t len)
{
    if (!device_is_ready(flash_dev))
    {
        return -ENODEV;
    }

    int rc = flash_write(flash_dev, offset, data, len);
    if (rc != 0)
    {
        LOG_ERR("flash_write failed at 0x%06X: %d", offset, rc);
        return rc;
    }

    bytes_used += len;
    k_msleep(50); /* allow chip's internal write cycle to complete */

    return 0;
}

/* Erase tuning constants — adjust here if needed for different flash chips */
#define ERASE_RETRIES 3   /* max retries if post-erase blank verify fails */
#define ERASE_WAIT_MS 500 /* forced wait after erase cmd before verifying */

int flash_storage_erase(uint32_t offset, uint32_t size)
{
    if (!device_is_ready(flash_dev))
    {
        return -ENODEV;
    }

    for (int attempt = 1; attempt <= ERASE_RETRIES; attempt++)
    {
        int rc = flash_erase(flash_dev, offset, size);

        /* Forced wait: driver completion does not guarantee the
         * chip has physically finished erasing internally. */
        k_msleep(ERASE_WAIT_MS);

        if (rc != 0)
        {
            LOG_WRN("Erase attempt %d returned %d, retrying...", attempt, rc);
            k_msleep(100);
            continue;
        }

        uint8_t check[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        size_t check_len = (size < sizeof(check)) ? size : sizeof(check);
        rc = flash_read(flash_dev, offset, check, check_len);

        bool blank = (rc == 0);
        for (size_t i = 0; blank && i < check_len; i++)
        {
            if (check[i] != 0xFF)
            {
                blank = false;
            }
        }

        if (blank)
        {
            bytes_used = (bytes_used >= size) ? (bytes_used - size) : 0;
            return 0;
        }

        LOG_WRN("Erase attempt %d did not verify blank, retrying...", attempt);
        k_msleep(100);
    }

    LOG_ERR("Erase at 0x%06X failed verification after %d attempts",
            offset, ERASE_RETRIES);
    return -EIO;
}