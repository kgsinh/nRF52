#ifndef SHOCK_LOG_H_
#define SHOCK_LOG_H_

#include "shock_detector/shock_detector.h"
#include <stdint.h>
#include <stdbool.h>

/** Maximum number of shock events stored before oldest is overwritten */
#define SHOCK_LOG_MAX_ENTRIES 512

/** Scale factor for storing m/s² as int16_t (2 decimal places) */
#define SHOCK_LOG_SCALE 100

/**
 * @brief Compact on-flash shock log entry — 16 bytes.
 *
 * All floating-point values are stored as fixed-point integers
 * (multiply by SHOCK_LOG_SCALE) to keep the struct small and
 * avoid float on the gateway decode side.
 */
struct __packed shock_log_entry
{
    int64_t timestamp_ms; /* k_uptime_get() at time of shock */
    int16_t accel_x;      /* m/s² × SHOCK_LOG_SCALE          */
    int16_t accel_y;
    int16_t accel_z;
    uint16_t magnitude; /* m/s² × SHOCK_LOG_SCALE          */
}; /* 16 bytes total                  */

/**
 * @brief Mount the LittleFS filesystem and open/create the log files.
 *
 * Must be called once at boot before any other shock_log_* function.
 *
 * @return 0 on success, negative errno on failure.
 */
int shock_log_init(void);

/**
 * @brief Write a shock event to the ring buffer.
 *
 * Overwrites the oldest entry when the log is full.
 *
 * @param event Pointer to the shock event to persist.
 * @return 0 on success, negative errno on failure.
 */
int shock_log_write(const struct shock_event *event);

/**
 * @brief Read one entry from the log by index.
 *
 * Index 0 = oldest entry, index (count-1) = newest.
 * Reading is non-destructive.
 *
 * @param index  Logical entry index (0 to count-1).
 * @param entry  Output buffer.
 * @return 0 on success, -ENOENT if index out of range.
 */
int shock_log_read(uint32_t index, struct shock_log_entry *entry);

/**
 * @brief Get the number of valid entries currently in the log.
 *
 * @return Entry count (0 to SHOCK_LOG_MAX_ENTRIES).
 */
uint32_t shock_log_count(void);

/**
 * @brief Erase all log entries and reset metadata.
 *
 * @return 0 on success, negative errno on failure.
 */
int shock_log_clear(void);

/**
 * @brief Get filesystem used and total bytes via LittleFS statvfs.
 *
 * @param used_bytes  Output — bytes currently used.
 * @param total_bytes Output — total filesystem capacity.
 * @return 0 on success, negative errno on failure.
 */
int shock_log_fs_stats(uint64_t *used_bytes, uint64_t *total_bytes);

#endif /* SHOCK_LOG_H_ */