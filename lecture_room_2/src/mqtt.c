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
#include "driver/gpio.h"

// EXTERNAL DECLARATIONS (Replaces wifi.h)
extern void init_wifi(void);
extern EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "ROOM_2_CONTROLLER";
#define ROOM_2_LIGHT_GPIO GPIO_NUM_5

// MQTT Event Handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Broker connected. Subscribing to Room 2 lights.");
            esp_mqtt_client_subscribe(client, "cocis/rooms/room2/lights", 1);
            break;
        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, "cocis/rooms/room2/lights", event->topic_len) == 0) {
                if (strncmp(event->data, "ON", event->data_len) == 0) {
                    gpio_set_level(ROOM_2_LIGHT_GPIO, 1);
                    ESP_LOGI(TAG, "Room 2 Lights Switched ON");
                } else if (strncmp(event->data, "OFF", event->data_len) == 0) {
                    gpio_set_level(ROOM_2_LIGHT_GPIO, 0);
                    ESP_LOGI(TAG, "Room 2 Lights Switched OFF");
                }
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

    gpio_reset_pin(ROOM_2_LIGHT_GPIO);
    gpio_set_direction(ROOM_2_LIGHT_GPIO, GPIO_MODE_OUTPUT);

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
            snprintf(payload, sizeof(payload), "{\"room\":\"room2\", \"occupancy\":false, \"light_status\":%d, \"uptime_ms\":%" PRIu64 "}", gpio_get_level(ROOM_2_LIGHT_GPIO), uptime);
            esp_mqtt_client_publish(client, "cocis/rooms/room2/occupancy", payload, 0, 1, 0);
            ESP_LOGI(TAG, "Room 2 telemetry status published.");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}