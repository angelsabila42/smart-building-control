#include "app_state.h"
#include "mqtt.h"

#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;
static atomic_int building_occupancy = ATOMIC_VAR_INIT(0);

int building_get_occupancy(void)
{
    return atomic_load(&building_occupancy);
}

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data)
{
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG, "Connected to MQTT broker");
            ESP_ERROR_CHECK(esp_mqtt_client_subscribe(
                mqtt_client,
                MQTT_BUILDING_OCCUPANCY_TOPIC,
                1
            ));
            break;

        case MQTT_EVENT_DATA: {
            if (event_data == NULL) {
                break;
            }

            esp_mqtt_event_handle_t event = event_data;
            if (event->topic_len != strlen(MQTT_BUILDING_OCCUPANCY_TOPIC) ||
                memcmp(
                    event->topic,
                    MQTT_BUILDING_OCCUPANCY_TOPIC,
                    event->topic_len
                ) != 0 ||
                event->data_len <= 0 ||
                event->data_len >= 12) {
                break;
            }

            char payload[12];
            memcpy(payload, event->data, event->data_len);
            payload[event->data_len] = '\0';

            char *end;
            long occupancy = strtol(payload, &end, 10);
            if (end == payload ||
                *end != '\0' ||
                occupancy < 0 ||
                occupancy > INT_MAX) {
                ESP_LOGW(TAG, "Invalid building occupancy: %s", payload);
                break;
            }

            atomic_store(&building_occupancy, (int)occupancy);
            ESP_LOGI(TAG, "Building occupancy=%ld", occupancy);
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            atomic_store(&building_occupancy, 0);
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT connection error");
            break;

        default:
            break;
    }
}


void start_mqtt(void)
{
    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = MQTT_URI,
        .credentials.client_id = "room2_controller"
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            mqtt_client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}