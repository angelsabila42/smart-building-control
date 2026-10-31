#include <stdio.h>
#include <stdbool.h>

// #include "app_config.h"
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
#define MOUNT_POINT "/sdcard"

static const char *TAG = "SD_CARD";

static sdmmc_card_t *sd_card = NULL;
static bool sd_mounted = false;
static SemaphoreHandle_t sd_mutex = NULL;

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

    spi_bus_config_t bus_config = {
        .mosi_io_num = SD_MOSI_PIN,
        .miso_io_num = SD_MISO_PIN,
        .sclk_io_num = SD_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000
    };

    esp_err_t ret = spi_bus_initialize(
        SPI2_HOST,
        &bus_config,
        SDSPI_DEFAULT_DMA
    );

    if (ret != ESP_OK) {
        if (ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "SPI initialization failed: %s",
                     esp_err_to_name(ret));
            xSemaphoreGive(sd_mutex);
            return ret;
        }
        ESP_LOGW(TAG, "SPI bus already initialized");
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

    ret = esp_vfs_fat_sdspi_mount(
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

    FILE *file = fopen(MOUNT_POINT "/cocis_log.csv", "a");

    if (file == NULL) {
        ESP_LOGE(TAG, "Could not open log file");
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

    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Event logged: %s", event);
    }
    xSemaphoreGive(sd_mutex);
    return result;
}
esp_err_t read_log(void)
{
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

    FILE *file = fopen(MOUNT_POINT "/cocis_log.csv", "r");

    if (file == NULL) {
        ESP_LOGE(TAG, "Could not open log file");
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }

    char line[256];

    printf("COCIS EVENT LOG\n");

    while (fgets(line, sizeof(line), file) != NULL) {
        printf("%s", line);
    }

    fclose(file);
    xSemaphoreGive(sd_mutex);
    return ESP_OK;
}
bool sd_card_is_mounted(void)
{
    return sd_mounted;
}