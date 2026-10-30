#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"

// EXTERNAL DECLARATIONS (Replaces wifi.h)
extern void init_wifi(void);
extern EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "ENTRANCE_CONTROLLER";

// MQTT Event Handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Broker connected.");
            esp_mqtt_client_subscribe(client, "cocis/entrance/schedule", 1);
            break;
        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, "cocis/entrance/schedule", event->topic_len) == 0) {
                ESP_LOGI(TAG, "Schedule configuration received: %.*s", event->data_len, event->data);
            }
            break;
        default:
            break;
    }
}

void init_mqtt(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_wifi(); // Calls function defined in wifi.c

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com", 
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    char payload[128]; 
    while (1) {
        if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) {
            uint64_t uptime = (uint64_t)(esp_timer_get_time() / 1000);
            snprintf(payload, sizeof(payload), "{\"terminal\":\"entrance\", \"status\":\"ONLINE\", \"uptime_ms\":%" PRIu64 "}", uptime);
            esp_mqtt_client_publish(client, "cocis/entrance/access", payload, 0, 1, 0);
            ESP_LOGI(TAG, "Entrance access log & heartbeat published.");
        } else {
            ESP_LOGE(TAG, "Skipped: Wi-Fi offline.");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}