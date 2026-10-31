#include "app_state.h"
#include "mqtt.h"

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;

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
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
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