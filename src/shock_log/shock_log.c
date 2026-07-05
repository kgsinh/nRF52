#include "shock_log/shock_log.h"

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <pm_config.h>
#include <string.h>
#include <math.h>

LOG_MODULE_REGISTER(shock_log, LOG_LEVEL_INF);

/* ── Filesystem mount ──────────────────────────────────────────────── */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_cfg);

static struct fs_mount_t lfs_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &lfs_cfg,
    .storage_dev = (void *)PM_EXTERNAL_FLASH_ID,
    .mnt_point = "/lfs",
};

/* ── File paths ────────────────────────────────────────────────────── */
#define LOG_FILE_PATH "/lfs/shock_log.bin"
#define META_FILE_PATH "/lfs/shock_meta.bin"

/* ── Metadata stored in shock_meta.bin ────────────────────────────── */
struct __packed shock_log_meta
{
    uint32_t version;
    uint32_t write_index;
    uint32_t count;
};

#define META_VERSION 1

/* ── Module state ──────────────────────────────────────────────────── */
static struct shock_log_meta meta = {0};
static bool mounted = false;

/* ── Internal helpers ──────────────────────────────────────────────── */
static int read_meta(void)
{
    struct fs_file_t f;
    fs_file_t_init(&f);

    int rc = fs_open(&f, META_FILE_PATH, FS_O_READ);
    if (rc != 0)
    {
        meta.version = META_VERSION;
        meta.write_index = 0;
        meta.count = 0;
        return 0;
    }

    ssize_t bytes = fs_read(&f, &meta, sizeof(meta));
    fs_close(&f);

    if (bytes != sizeof(meta) || meta.version != META_VERSION)
    {
        LOG_WRN("Metadata invalid — resetting log");
        meta.version = META_VERSION;
        meta.write_index = 0;
        meta.count = 0;
    }

    return 0;
}

static int write_meta(void)
{
    struct fs_file_t f;
    fs_file_t_init(&f);

    int rc = fs_open(&f, META_FILE_PATH,
                     FS_O_WRITE | FS_O_CREATE | FS_O_TRUNC);
    if (rc != 0)
    {
        LOG_ERR("Failed to open meta file for write: %d", rc);
        return rc;
    }

    ssize_t written = fs_write(&f, &meta, sizeof(meta));
    fs_close(&f);

    if (written != sizeof(meta))
    {
        LOG_ERR("Meta write incomplete: %d of %u bytes",
                written, sizeof(meta));
        return -EIO;
    }

    return 0;
}

/* ── Public API ────────────────────────────────────────────────────── */
int shock_log_init(void)
{
    int rc = fs_mount(&lfs_mnt);
    if (rc != 0)
    {
        LOG_ERR("LittleFS mount failed: %d", rc);
        return rc;
    }

    mounted = true;
    LOG_INF("LittleFS mounted at %s", lfs_mnt.mnt_point);

    rc = read_meta();
    if (rc != 0)
    {
        return rc;
    }

    LOG_INF("Shock log ready: %u entries (max %u)",
            meta.count, SHOCK_LOG_MAX_ENTRIES);

    return 0;
}

int shock_log_write(const struct shock_event *event)
{
    if (!mounted || event == NULL)
    {
        return -ENODEV;
    }

    struct shock_log_entry entry = {
        .timestamp_ms = event->timestamp_ms,
        .accel_x = (int16_t)(sensor_value_to_double(&event->sample.accel[0]) * SHOCK_LOG_SCALE),
        .accel_y = (int16_t)(sensor_value_to_double(&event->sample.accel[1]) * SHOCK_LOG_SCALE),
        .accel_z = (int16_t)(sensor_value_to_double(&event->sample.accel[2]) * SHOCK_LOG_SCALE),
        .magnitude = (uint16_t)(event->magnitude * SHOCK_LOG_SCALE),
    };

    struct fs_file_t f;
    fs_file_t_init(&f);

    int rc = fs_open(&f, LOG_FILE_PATH, FS_O_RDWR | FS_O_CREATE);
    if (rc != 0)
    {
        LOG_ERR("Failed to open log file: %d", rc);
        return rc;
    }

    off_t offset = (off_t)(meta.write_index * sizeof(struct shock_log_entry));
    rc = fs_seek(&f, offset, FS_SEEK_SET);
    if (rc != 0)
    {
        LOG_ERR("fs_seek failed: %d", rc);
        fs_close(&f);
        return rc;
    }

    ssize_t written = fs_write(&f, &entry, sizeof(entry));
    fs_close(&f);

    if (written != sizeof(entry))
    {
        LOG_ERR("Log write incomplete: %d of %u bytes",
                written, sizeof(entry));
        return -EIO;
    }

    meta.write_index = (meta.write_index + 1) % SHOCK_LOG_MAX_ENTRIES;
    if (meta.count < SHOCK_LOG_MAX_ENTRIES)
    {
        meta.count++;
    }

    return write_meta();
}

int shock_log_read(uint32_t index, struct shock_log_entry *entry)
{
    if (!mounted || entry == NULL || index >= meta.count)
    {
        return -ENOENT;
    }

    uint32_t physical;
    if (meta.count < SHOCK_LOG_MAX_ENTRIES)
    {
        physical = index;
    }
    else
    {
        physical = (meta.write_index + index) % SHOCK_LOG_MAX_ENTRIES;
    }

    struct fs_file_t f;
    fs_file_t_init(&f);

    int rc = fs_open(&f, LOG_FILE_PATH, FS_O_READ);
    if (rc != 0)
    {
        return rc;
    }

    off_t offset = (off_t)(physical * sizeof(struct shock_log_entry));
    rc = fs_seek(&f, offset, FS_SEEK_SET);
    if (rc != 0)
    {
        fs_close(&f);
        return rc;
    }

    ssize_t bytes = fs_read(&f, entry, sizeof(*entry));
    fs_close(&f);

    return (bytes == sizeof(*entry)) ? 0 : -EIO;
}

uint32_t shock_log_count(void)
{
    return meta.count;
}

int shock_log_clear(void)
{
    if (!mounted)
    {
        return -ENODEV;
    }

    fs_unlink(LOG_FILE_PATH);
    fs_unlink(META_FILE_PATH);

    meta.version = META_VERSION;
    meta.write_index = 0;
    meta.count = 0;

    LOG_INF("Shock log cleared");
    return 0;
}

int shock_log_fs_stats(uint64_t *used_bytes, uint64_t *total_bytes)
{
    if (!mounted || used_bytes == NULL || total_bytes == NULL)
    {
        return -EINVAL;
    }

    struct fs_statvfs stat;
    int rc = fs_statvfs(lfs_mnt.mnt_point, &stat);
    if (rc != 0)
    {
        LOG_ERR("fs_statvfs failed: %d", rc);
        return rc;
    }

    *total_bytes = (uint64_t)stat.f_blocks * stat.f_frsize;
    *used_bytes = (uint64_t)(stat.f_blocks - stat.f_bfree) * stat.f_frsize;

    return 0;
}