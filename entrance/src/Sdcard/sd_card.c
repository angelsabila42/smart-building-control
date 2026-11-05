#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "sd_card.h"

#include "esp_log.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SD_MOSI_PIN 35
#define SD_MISO_PIN 37
#define SD_SCLK_PIN 36
#define SD_CS_PIN 7


static const char *TAG = "SD_CARD";

static sdmmc_card_t *sd_card = NULL;
static bool sd_mounted = false;
static SemaphoreHandle_t sd_mutex = NULL;

//Filters events
static const char *get_log_file(const char *event){
    if (strncmp(event, "ENTRY_", strlen("ENTRY_")) == 0 ||
        strncmp(event, "EXIT_", strlen("EXIT_")) == 0) {
        return ACCESS_LOG_FILE;
    }

    if (strcmp(event, "ROOM1_STATUS") == 0 ||
        strcmp(event, "ROOM2_STATUS") == 0) {
        return ROOM_LOG_FILE;
    }

    if (strcmp(event, "SCHEDULE_SET") == 0 ||
        strcmp(event, "USER_ADDED") == 0 ||
        strcmp(event, "USER_REMOVED") == 0 ||
        strcmp(event, "USER_AUTHORIZED") == 0 ||
        strcmp(event, "USER_REVOKED") == 0 ||
        strcmp(event, "LOG_FLUSH") == 0) {
        return ADMIN_LOG_FILE;
    }

    if (strcmp(event, "MAIN") == 0 || strcmp(event, "ALARM") == 0) {
        return SYSTEM_LOG_FILE;
    }

    ESP_LOGW(TAG, "Unrecognized log event '%s'; routing it to the system log", event);
    return SYSTEM_LOG_FILE;
   

}

static esp_err_t print_log_file(const char *path, bool optional, void (*print_fn)(const char *)){
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        if (optional && errno == ENOENT) {
            return ESP_OK;
        }

        ESP_LOGE(TAG, "Could not open log file %s", path);
        return ESP_FAIL;
    }

    char line[256];
    esp_err_t result = ESP_OK;

    while (fgets(line, sizeof(line), file) != NULL) {

        line[strcspn(line, "\r\n")] = '\0';
        print_fn(line);
    }

    if (ferror(file)) {
        ESP_LOGE(TAG, "Failed while reading log file %s", path);
        result = ESP_FAIL;
    }

    if (fclose(file) != 0) {
        ESP_LOGE(TAG, "Failed to close log file %s", path);
        result = ESP_FAIL;
    }

    return result;


}


esp_err_t sd_card_init(void)
{
    if (sd_mutex == NULL) {
        sd_mutex = xSemaphoreCreateMutex();

        if (sd_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create SD-card mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    xSemaphoreTake(sd_mutex, portMAX_DELAY);

    if (sd_mounted) {             //Avoid initializing the card again if already mounted
        xSemaphoreGive(sd_mutex);
        ESP_LOGI(TAG, "SD card is already mounted");
        return ESP_OK;
    }


    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SPI2_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    esp_err_t ret = esp_vfs_fat_sdspi_mount(
        MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &sd_card
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD-card mount failed: %s", esp_err_to_name(ret));
        xSemaphoreGive(sd_mutex);
        return ret;
    }

    sd_mounted = true;

    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, sd_card);
    xSemaphoreGive(sd_mutex);
    return ESP_OK;
}

esp_err_t log_event(const char *event, const char *details)
{
    if (event == NULL || details == NULL) {
        ESP_LOGE(TAG, "Event or details cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (sd_mutex == NULL) {
        ESP_LOGW(TAG, "SD-card module has not been initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timed out waiting to write log");
        return ESP_ERR_TIMEOUT;
    }

    if (!sd_mounted) {
        ESP_LOGW(TAG, "SD card is not mounted");
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }

    const char *log_file = get_log_file(event);
    FILE *file = fopen(log_file, "a");

    if (file == NULL) {
        ESP_LOGE(TAG, "Could not open log file %s", log_file);
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }

    esp_err_t result = ESP_OK;

    if (fprintf(file, "%s,%s\n", event, details) < 0) {
        ESP_LOGE(TAG, "Failed to write log entry");
        result = ESP_FAIL;
    }

    if (fflush(file) != 0) {
        ESP_LOGE(TAG, "Failed to flush log entry");
        result = ESP_FAIL;
    }

    if (fclose(file) != 0) {
        ESP_LOGE(TAG, "Failed to close log file");
        result = ESP_FAIL;
    }

    xSemaphoreGive(sd_mutex);
    return result;
}
esp_err_t read_log(const char *path, void (*print_fn)(const char *))
{
    if (print_fn == NULL) {
        ESP_LOGE(TAG, "Print callback cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (sd_mutex == NULL) {
        ESP_LOGE(TAG, "SD-card module has not been initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timed out waiting to read log");
        return ESP_ERR_TIMEOUT;
    }

    if (!sd_mounted) {
        ESP_LOGE(TAG, "SD card is not mounted");
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }

    print_fn("--- START OF LOG ---");
    esp_err_t result = print_log_file(path, true, print_fn);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read log file %s", path);
    }
    print_fn("--- END OF LOG ---");

    xSemaphoreGive(sd_mutex);
    return result;
}
bool sd_card_is_mounted(void)
{
    return sd_mounted;
}