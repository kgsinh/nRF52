#ifndef FLASH_STORAGE_H_
#define FLASH_STORAGE_H_

#include <zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the flash storage module.
 *
 * Confirms the underlying flash device is ready and caches its
 * total size. Must be called once before any other flash_storage_*
 * function.
 *
 * @return 0 on success, negative errno on failure.
 */
int flash_storage_init(void);

/**
 * @brief Read the chip's JEDEC manufacturer/device ID.
 *
 * @param id Output buffer, must be at least 3 bytes.
 * @return 0 on success, negative errno on failure.
 */
int flash_storage_check_id(uint8_t id[3]);

/**
 * @brief Get the chip's total capacity in bytes.
 *
 * @return Total size in bytes, or 0 if not yet initialized.
 */
uint64_t flash_storage_get_total_size(void);

/**
 * @brief Get the number of bytes currently tracked as used.
 *
 * This is a software-only counter maintained by this module —
 * it does not reflect a filesystem's real allocation state.
 *
 * @return Bytes used.
 */
uint64_t flash_storage_get_used(void);

/**
 * @brief Get the number of bytes currently tracked as available.
 *
 * @return Bytes available (total - used).
 */
uint64_t flash_storage_get_available(void);

/**
 * @brief Print a usage summary line to the log.
 *
 * @param label Short label identifying the call site (e.g. "After write").
 */
void flash_storage_print_usage(const char *label);

/**
 * @brief Read bytes from flash at the given offset.
 *
 * @param offset Byte offset within the flash device.
 * @param data Output buffer.
 * @param len Number of bytes to read.
 * @return 0 on success, negative errno on failure.
 */
int flash_storage_read(uint32_t offset, uint8_t *data, size_t len);

/**
 * @brief Write bytes to flash at the given offset.
 *
 * The target region must already be erased. On success, the
 * internal used-byte counter is incremented by @p len.
 *
 * @param offset Byte offset within the flash device.
 * @param data Data to write.
 * @param len Number of bytes to write.
 * @return 0 on success, negative errno on failure.
 */
int flash_storage_write(uint32_t offset, const uint8_t *data, size_t len);

/**
 * @brief Erase a region of flash, with retry and verification.
 *
 * Retries up to 3 times, waiting 500ms after each erase attempt
 * before verifying the region reads back as all 0xFF. On success,
 * the internal used-byte counter is decremented by @p size
 * (clamped to zero).
 *
 * @param offset Byte offset, must be erase-sector aligned.
 * @param size Number of bytes to erase (typically 4096).
 * @return 0 on success, -EIO if verification failed after retries.
 */
int flash_storage_erase(uint32_t offset, uint32_t size);

#endif /* FLASH_STORAGE_H_ */