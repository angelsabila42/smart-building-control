#include "management/management_store.h"

#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define STORE_TAG "MGMT_STORE"
#define STORE_NAMESPACE "cocis_cfg"
#define STORE_KEY_SCHEDULE "schedule"
#define STORE_KEY_CREDENTIALS "creds"

#define STORE_SCHEDULE_MAGIC 0x53434845u /* "SCHE" */
#define STORE_SCHEDULE_VERSION 1u

#define STORE_CREDENTIALS_MAGIC 0x43524544u /* "CRED" */
#define STORE_CREDENTIALS_VERSION 1u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    access_schedule_t schedule;
} stored_schedule_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    admin_credentials_t credentials;
} stored_credentials_t;

static bool open_storage(nvs_open_mode_t mode, nvs_handle_t *handle)
{
    const esp_err_t result = nvs_open(STORE_NAMESPACE, mode, handle);

    if (result != ESP_OK) {
        ESP_LOGW(STORE_TAG, "Cannot open NVS namespace: %s", esp_err_to_name(result));
        return false;
    }

    return true;
}

static bool read_blob(nvs_handle_t handle, const char *key, void *destination, size_t size)
{
    size_t stored_size = size;
    const esp_err_t result = nvs_get_blob(handle, key, destination, &stored_size);

    if (result != ESP_OK || stored_size != size) {
        if (result != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(STORE_TAG,
                     "Cannot read '%s': %s (size %u/%u)",
                     key,
                     esp_err_to_name(result),
                     (unsigned)stored_size,
                     (unsigned)size);
        }

        return false;
    }

    return true;
}

bool management_store_load(access_schedule_t *schedule, admin_credentials_t *credentials)
{
    if (schedule == NULL || credentials == NULL) {
        return false;
    }

    nvs_handle_t handle;

    if (!open_storage(NVS_READONLY, &handle)) {
        return false;
    }

    stored_schedule_t stored_schedule;

    if (read_blob(handle, STORE_KEY_SCHEDULE, &stored_schedule, sizeof(stored_schedule)) &&
        stored_schedule.magic == STORE_SCHEDULE_MAGIC &&
        stored_schedule.version == STORE_SCHEDULE_VERSION) {
        char error[96];

        if (access_schedule_validate(&stored_schedule.schedule, error, sizeof(error)) ==
            ACCESS_SCHEDULE_OK) {
            *schedule = stored_schedule.schedule;
        } else {
            ESP_LOGW(STORE_TAG, "Stored schedule is invalid: %s", error);
        }
    }

    stored_credentials_t stored_credentials;

    if (read_blob(handle,
                  STORE_KEY_CREDENTIALS,
                  &stored_credentials,
                  sizeof(stored_credentials)) &&
        stored_credentials.magic == STORE_CREDENTIALS_MAGIC &&
        stored_credentials.version == STORE_CREDENTIALS_VERSION &&
        stored_credentials.credentials.provisioned &&
        stored_credentials.credentials.username[0] != '\0' &&
        stored_credentials.credentials.iterations > 0) {
        *credentials = stored_credentials.credentials;
    }

    nvs_close(handle);
    return true;
}

bool management_store_save(const access_schedule_t *schedule,
                           const admin_credentials_t *credentials)
{
    if (schedule == NULL || credentials == NULL) {
        return false;
    }

    nvs_handle_t handle;

    if (!open_storage(NVS_READWRITE, &handle)) {
        return false;
    }

    stored_schedule_t stored_schedule = {
        .magic = STORE_SCHEDULE_MAGIC,
        .version = STORE_SCHEDULE_VERSION,
        .reserved = 0,
        .schedule = *schedule,
    };

    stored_credentials_t stored_credentials = {
        .magic = STORE_CREDENTIALS_MAGIC,
        .version = STORE_CREDENTIALS_VERSION,
        .reserved = 0,
        .credentials = *credentials,
    };

    esp_err_t result = nvs_set_blob(handle, STORE_KEY_SCHEDULE, &stored_schedule, sizeof(stored_schedule));

    if (result == ESP_OK) {
        result = nvs_set_blob(handle,
                              STORE_KEY_CREDENTIALS,
                              &stored_credentials,
                              sizeof(stored_credentials));
    }

    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }

    nvs_close(handle);

    if (result != ESP_OK) {
        ESP_LOGE(STORE_TAG, "Config save failed: %s", esp_err_to_name(result));
        return false;
    }

    return true;
}
