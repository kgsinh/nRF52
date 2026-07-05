#include "ble_service/ble_service.h"
#include "shock_log/shock_log.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(ble_service, LOG_LEVEL_INF);

/* ── UUIDs ─────────────────────────────────────────────────────────
 * Base: 12345678-1234-5678-1234-56789abcdef0
 */
#define BT_UUID_ASSET_TRACKER_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BT_UUID_SENSOR_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define BT_UUID_SHOCK_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)
#define BT_UUID_CONFIG_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)
#define BT_UUID_FLASH_STATUS_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4)
#define BT_UUID_LOG_READBACK_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef5)

#define BT_UUID_ASSET_TRACKER BT_UUID_DECLARE_128(BT_UUID_ASSET_TRACKER_VAL)
#define BT_UUID_SENSOR BT_UUID_DECLARE_128(BT_UUID_SENSOR_VAL)
#define BT_UUID_SHOCK BT_UUID_DECLARE_128(BT_UUID_SHOCK_VAL)
#define BT_UUID_CONFIG BT_UUID_DECLARE_128(BT_UUID_CONFIG_VAL)
#define BT_UUID_FLASH_STATUS BT_UUID_DECLARE_128(BT_UUID_FLASH_STATUS_VAL)
#define BT_UUID_LOG_READBACK BT_UUID_DECLARE_128(BT_UUID_LOG_READBACK_VAL)

/* ── Wire payloads ─────────────────────────────────────────────────*/
struct __packed ble_sensor_payload
{
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
};

struct __packed ble_shock_payload
{
    int64_t timestamp_ms;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int32_t magnitude;
};

struct __packed ble_flash_status_payload
{
    uint32_t used_bytes;
    uint32_t total_bytes;
};

/* ── Module state ──────────────────────────────────────────────────*/
static struct bt_conn *current_conn = NULL;
static bool sensor_notify_enabled = false;
static bool shock_notify_enabled = false;
static bool log_readback_notify_enabled = false;
static float config_threshold = SHOCK_THRESHOLD_DEFAULT_MS2;

/* Semaphore signalled by the GATT write handler to trigger readback */
K_SEM_DEFINE(readback_sem, 0, 1);

/* ── CCCD callbacks ────────────────────────────────────────────────*/
static void sensor_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    sensor_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Live sensor notifications: %s",
            sensor_notify_enabled ? "enabled" : "disabled");
}

static void shock_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    shock_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Shock event notifications: %s",
            shock_notify_enabled ? "enabled" : "disabled");
}

static void log_readback_ccc_changed(const struct bt_gatt_attr *attr,
                                     uint16_t value)
{
    log_readback_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Log readback notifications: %s",
            log_readback_notify_enabled ? "enabled" : "disabled");
}

/* ── Read handlers ─────────────────────────────────────────────────*/
static ssize_t read_flash_status(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 void *buf, uint16_t len, uint16_t offset)
{
    struct ble_flash_status_payload payload = {0};
    uint64_t used = 0, total = 0;

    if (shock_log_fs_stats(&used, &total) == 0)
    {
        payload.used_bytes = (uint32_t)used;
        payload.total_bytes = (uint32_t)total;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &payload, sizeof(payload));
}

static ssize_t read_config(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &config_threshold, sizeof(config_threshold));
}

/* ── Write handlers ────────────────────────────────────────────────*/
static ssize_t write_config(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len,
                            uint16_t offset, uint8_t flags)
{
    if (len != sizeof(float))
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    memcpy(&config_threshold, buf, sizeof(float));
    shock_detector_set_threshold(config_threshold);
    LOG_INF("Threshold updated via BLE: %.2f m/s²", (double)config_threshold);
    return len;
}

static ssize_t write_log_readback(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  const void *buf, uint16_t len,
                                  uint16_t offset, uint8_t flags)
{
    if (len < 1)
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t cmd = ((const uint8_t *)buf)[0];

    if (cmd == 0x01)
    {
        LOG_INF("Log readback requested by client");
        k_sem_give(&readback_sem);
    }

    return len;
}

/* ── GATT service definition ───────────────────────────────────────*/
BT_GATT_SERVICE_DEFINE(asset_tracker_svc,
                       /* Primary service */
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_ASSET_TRACKER),

                       /* 1. Live sensor — READ + NOTIFY (attrs 1,2,3) */
                       BT_GATT_CHARACTERISTIC(BT_UUID_SENSOR,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              NULL, NULL, NULL),
                       BT_GATT_CCC(sensor_ccc_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

                       /* 2. Shock event — READ + NOTIFY (attrs 4,5,6) */
                       BT_GATT_CHARACTERISTIC(BT_UUID_SHOCK,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              NULL, NULL, NULL),
                       BT_GATT_CCC(shock_ccc_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

                       /* 3. Config — READ + WRITE (attrs 7,8) */
                       BT_GATT_CHARACTERISTIC(BT_UUID_CONFIG,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                                              read_config, write_config, &config_threshold),

                       /* 4. Flash status — READ only (attrs 9,10) */
                       BT_GATT_CHARACTERISTIC(BT_UUID_FLASH_STATUS,
                                              BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ,
                                              read_flash_status, NULL, NULL),

                       /* 5. Log readback — WRITE + NOTIFY (attrs 11,12,13) */
                       BT_GATT_CHARACTERISTIC(BT_UUID_LOG_READBACK,
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_WRITE,
                                              NULL, write_log_readback, NULL),
                       BT_GATT_CCC(log_readback_ccc_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

/* ── Log readback thread ───────────────────────────────────────────*/
static void log_readback_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1)
    {
        /* Block until client requests a readback */
        k_sem_take(&readback_sem, K_FOREVER);

        if (current_conn == NULL || !log_readback_notify_enabled)
        {
            LOG_WRN("Readback requested but client not ready");
            continue;
        }

        uint32_t count = shock_log_count();
        LOG_INF("Starting log readback: %u entries", count);

        if (count == 0)
        {
            /* Send one empty notification to signal no data */
            struct ble_log_entry_payload empty = {
                .index = 0,
                .total = 0,
            };
            const struct bt_gatt_attr *attr = &asset_tracker_svc.attrs[12];
            bt_gatt_notify(current_conn, attr, &empty, sizeof(empty));
            LOG_INF("Readback complete: log empty");
            continue;
        }

        const struct bt_gatt_attr *attr = &asset_tracker_svc.attrs[12];

        for (uint32_t i = 0; i < count; i++)
        {
            struct shock_log_entry entry;
            int rc = shock_log_read(i, &entry);
            if (rc != 0)
            {
                LOG_WRN("Read entry %u failed: %d", i, rc);
                continue;
            }

            struct ble_log_entry_payload payload = {
                .index = (uint16_t)i,
                .total = (uint16_t)count,
                .timestamp_ms = entry.timestamp_ms,
                .accel_x = entry.accel_x,
                .accel_y = entry.accel_y,
                .accel_z = entry.accel_z,
                .magnitude = entry.magnitude,
            };

            rc = bt_gatt_notify(current_conn, attr,
                                &payload, sizeof(payload));
            if (rc != 0)
            {
                LOG_WRN("Notify failed at entry %u: %d", i, rc);
            }

            /* Brief yield between notifications to avoid flooding
               the BLE stack TX buffer */
            k_msleep(10);
        }

        LOG_INF("Readback complete: %u entries sent", count);
    }
}

K_THREAD_DEFINE(readback_thread,
                2048,
                log_readback_thread_fn,
                NULL, NULL, NULL,
                8, /* lower priority than shock_detector */
                0,
                0);

/* ── Connection callbacks ──────────────────────────────────────────*/
static void on_connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_ERR("Connection failed: %u", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("Client connected — %u shock events stored",
            shock_log_count());
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Client disconnected (reason: %u)", reason);
    bt_conn_unref(current_conn);
    current_conn = NULL;
    sensor_notify_enabled = false;
    shock_notify_enabled = false;
    log_readback_notify_enabled = false;

    int rc = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, NULL, 0, NULL, 0);
    if (rc != 0)
    {
        LOG_ERR("Failed to restart advertising: %d", rc);
    }
    else
    {
        LOG_INF("Advertising restarted");
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

/* ── Advertising data ──────────────────────────────────────────────*/
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
                  BT_UUID_ASSET_TRACKER_VAL),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* ── Public API ────────────────────────────────────────────────────*/
int ble_service_init(void)
{
    int rc = bt_enable(NULL);
    if (rc != 0)
    {
        LOG_ERR("bt_enable failed: %d", rc);
        return rc;
    }

    rc = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                         ad, ARRAY_SIZE(ad),
                         sd, ARRAY_SIZE(sd));
    if (rc != 0)
    {
        LOG_ERR("bt_le_adv_start failed: %d", rc);
        return rc;
    }

    LOG_INF("BLE advertising started — name: \"%s\"",
            CONFIG_BT_DEVICE_NAME);
    return 0;
}

bool ble_service_is_connected(void)
{
    return (current_conn != NULL);
}

void ble_service_notify_shock(const struct shock_event *event)
{
    if (!shock_notify_enabled || current_conn == NULL)
    {
        return;
    }

    struct ble_shock_payload payload = {
        .timestamp_ms = event->timestamp_ms,
        .accel_x = (int16_t)(sensor_value_to_double(
                                 &event->sample.accel[0]) *
                             1000),
        .accel_y = (int16_t)(sensor_value_to_double(
                                 &event->sample.accel[1]) *
                             1000),
        .accel_z = (int16_t)(sensor_value_to_double(
                                 &event->sample.accel[2]) *
                             1000),
        .magnitude = (int32_t)(event->magnitude * 1000),
    };

    /* Shock characteristic value is at attr index 5 */
    const struct bt_gatt_attr *attr = &asset_tracker_svc.attrs[5];
    bt_gatt_notify(current_conn, attr, &payload, sizeof(payload));
}

void ble_service_notify_sensor(const struct imu_sample *sample)
{
    if (!sensor_notify_enabled || current_conn == NULL)
    {
        return;
    }

    struct ble_sensor_payload payload = {
        .accel_x = (int16_t)(sensor_value_to_double(
                                 &sample->accel[0]) *
                             1000),
        .accel_y = (int16_t)(sensor_value_to_double(
                                 &sample->accel[1]) *
                             1000),
        .accel_z = (int16_t)(sensor_value_to_double(
                                 &sample->accel[2]) *
                             1000),
        .gyro_x = (int16_t)(sensor_value_to_double(
                                &sample->gyro[0]) *
                            1000),
        .gyro_y = (int16_t)(sensor_value_to_double(
                                &sample->gyro[1]) *
                            1000),
        .gyro_z = (int16_t)(sensor_value_to_double(
                                &sample->gyro[2]) *
                            1000),
    };

    /* Sensor characteristic value is at attr index 2 */
    const struct bt_gatt_attr *attr = &asset_tracker_svc.attrs[2];
    bt_gatt_notify(current_conn, attr, &payload, sizeof(payload));
}