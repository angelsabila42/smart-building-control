#include "management/management_app.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "management/access_events.h"
#include "management/access_schedule.h"
#include "management/admin_auth.h"
#include "management/management_console.h"
#include "management/management_store.h"
#include "management/serial_menu.h"
#include "management/system_status.h"
#include "management/time_utils.h"

#define APP_TAG "MGMT"
#define APP_FIRMWARE "0.1.0"
#define ROOM_STATUS_TIMEOUT_SECONDS 120

static access_schedule_t scheduling;
static admin_auth_t authentication;
static access_events_t access_history;
static system_status_t status_snapshot;

static SemaphoreHandle_t access_history_mutex = NULL;
static int64_t boot_monotonic_seconds = 0;
static bool app_ready = false;

static void (*audit_hook)(const char *event, const char *details) = NULL;

static int64_t monotonic_now(void)
{
    return (int64_t)(esp_timer_get_time() / 1000000LL);
}

static int64_t epoch_now(void)
{
    return (int64_t)time(NULL);
}

static bool clock_read(struct tm *out)
{
    const int64_t epoch = epoch_now();

    if (!time_epoch_is_plausible(epoch)) {
        return false;
    }

    time_epoch_to_tm(epoch, out);
    return true;
}

static bool clock_write(const struct tm *value)
{
    const time_t epoch = (time_t)time_tm_to_epoch(value);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };

    return settimeofday(&tv, NULL) == 0;
}

static int64_t menu_epoch(void *context)
{
    (void)context;
    return epoch_now();
}

static int64_t menu_monotonic(void *context)
{
    (void)context;
    return monotonic_now();
}

static void menu_refresh(void *context)
{
    (void)context;
    management_app_refresh();
}

static void hardware_random(void *context, uint8_t *buffer, size_t length)
{
    (void)context;
    esp_fill_random(buffer, length);
}

static void history_lock(void *context)
{
    if (context != NULL) {
        xSemaphoreTake((SemaphoreHandle_t)context, portMAX_DELAY);
    }
}

static void history_unlock(void *context)
{
    if (context != NULL) {
        xSemaphoreGive((SemaphoreHandle_t)context);
    }
}

static void audit_log(void *context, const char *event, const char *details)
{
    (void)context;

    ESP_LOGI(APP_TAG, "AUDIT %s: %s", event, details != NULL ? details : "");

    if (audit_hook != NULL) {
        audit_hook(event, details);
    }
}

static bool save_configuration(void *context)
{
    (void)context;
    return management_store_save(&scheduling, &authentication.credentials);
}

void management_app_refresh(void)
{
    if (!app_ready) {
        return;
    }

    const int64_t now = epoch_now();

    status_snapshot.uptime_seconds = monotonic_now() - boot_monotonic_seconds;
    status_snapshot.free_heap = esp_get_free_heap_size();
    status_snapshot.min_free_heap = esp_get_minimum_free_heap_size();
    status_snapshot.clock_valid = time_epoch_is_plausible(now);
    status_snapshot.schedule_enabled = scheduling.schedule_enabled;
    status_snapshot.override_active = access_schedule_override_active(&scheduling, now);
}

static bool nvs_ready(void)
{
    esp_err_t result = nvs_flash_init();

    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(APP_TAG, "Erasing NVS and retrying (%s)", esp_err_to_name(result));
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    if (result != ESP_OK) {
        ESP_LOGE(APP_TAG, "NVS unavailable: %s", esp_err_to_name(result));
        return false;
    }

    return true;
}

void management_app_start(void)
{
    const bool storage_available = nvs_ready();

    access_schedule_defaults(&scheduling);

    memset(&authentication, 0, sizeof(authentication));
    admin_auth_set_random_source(&authentication, hardware_random, NULL);
    admin_auth_init(&authentication, 0);

    if (storage_available) {
        admin_credentials_t stored_credentials;

        memset(&stored_credentials, 0, sizeof(stored_credentials));

        if (management_store_load(&scheduling, &stored_credentials) &&
            stored_credentials.provisioned) {
            authentication.credentials = stored_credentials;
            ESP_LOGI(APP_TAG, "Restored credentials for '%s'", stored_credentials.username);
        }
    }

    access_events_init(&access_history);
    access_history_mutex = xSemaphoreCreateMutex();

    if (access_history_mutex != NULL) {
        access_events_set_lock(&access_history,
                               history_lock,
                               history_unlock,
                               access_history_mutex);
    } else {
        ESP_LOGW(APP_TAG, "No mutex for access history; console/RFID races possible");
    }

    system_status_init(&status_snapshot, APP_FIRMWARE);

    boot_monotonic_seconds = monotonic_now();
    app_ready = true;

    management_app_refresh();

    serial_menu_config_t menu_config = {
        .context = NULL,
        .write = management_console_write,
        .save_config = save_configuration,
        .audit = audit_log,
        .get_wall_clock = clock_read,
        .set_wall_clock = clock_write,
        .now_epoch = menu_epoch,
        .monotonic_seconds = menu_monotonic,
        .refresh_status = menu_refresh,
        .schedule = &scheduling,
        .auth = &authentication,
        .events = &access_history,
        .status = &status_snapshot,
    };

    if (!serial_menu_init(&menu_config)) {
        ESP_LOGE(APP_TAG, "Serial menu initialisation failed");
        return;
    }

    struct tm probe;

    if (!clock_read(&probe)) {
        ESP_LOGW(APP_TAG, "Clock is not set; access decisions fail closed until 'time set'");
    }

    management_console_start();

    ESP_LOGI(APP_TAG, "Management interface ready (firmware %s)", APP_FIRMWARE);
}

bool management_app_ready(void)
{
    return app_ready;
}

bool management_access_check(const char *uid, char direction)
{
    if (!app_ready) {
        return false;
    }

    const bool is_exit = (direction == 'O' || direction == 'o');
    const int64_t now = epoch_now();
    struct tm wall_clock;

    const bool clock_ok = clock_read(&wall_clock);

    access_decision_t decision = is_exit
                                     ? access_schedule_check_exit()
                                     : access_schedule_check(&scheduling,
                                                             clock_ok ? &wall_clock : NULL,
                                                             now);

    const bool allowed = access_decision_is_allowed(decision);

    access_event_t event;

    memset(&event, 0, sizeof(event));
    event.timestamp = clock_ok ? now : 0;
    event.direction = is_exit ? 'O' : 'I';
    event.allowed = allowed;

    if (is_exit) {
        snprintf(event.reason, sizeof(event.reason), "%s", "egress is never blocked");
    } else {
        snprintf(event.reason,
                 sizeof(event.reason),
                 "%s",
                 access_decision_reason(decision));
    }

    snprintf(event.uid, sizeof(event.uid), "%s", uid != NULL ? uid : "unknown");
    access_events_add(&access_history, &event);

    if (allowed) {
        if (is_exit) {
            if (status_snapshot.building_occupancy > 0) {
                status_snapshot.building_occupancy--;
            }
        } else {
            status_snapshot.building_occupancy++;
        }

        status_snapshot.occupancy_known = true;
        status_snapshot.last_access_event = event.timestamp;
    }

    char details[128];

    snprintf(details,
             sizeof(details),
             "uid=%s direction=%c %s reason=%s",
             event.uid,
             event.direction,
             allowed ? "GRANTED" : "DENIED",
             event.reason);

    audit_log(NULL, allowed ? "ACCESS_GRANTED" : "ACCESS_DENIED", details);

    return allowed;
}

bool management_on_card_seen(const char *uid)
{
    return management_access_check(uid, 'I');
}

void management_app_set_storage(bool mounted)
{
    status_snapshot.sd_mounted = mounted;
}

void management_app_set_connectivity(bool wifi_connected, bool mqtt_connected)
{
    status_snapshot.wifi_connected = wifi_connected;
    status_snapshot.mqtt_connected = mqtt_connected;
}

void management_app_update_room(int room_index, bool occupied, bool light_on)
{
    system_status_room_update(&status_snapshot, room_index, occupied, light_on, epoch_now());
}

void management_app_set_audit_hook(void (*hook)(const char *event, const char *details))
{
    audit_hook = hook;
}
