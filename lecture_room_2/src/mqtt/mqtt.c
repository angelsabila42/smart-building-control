#include "app_state.h"
#include "mqtt.h"
#include "controller.h"

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;
static char mqtt_client_id[32];

static void subscribe_to_topic(const char *topic)
{
    int message_id = esp_mqtt_client_subscribe(mqtt_client, topic, 1);
    if (message_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to %s", topic);
    }
}

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG, "Connected to MQTT broker");
            subscribe_to_topic(MQTT_PEER_TOPIC);
            publish_current_room_status();
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;

        // case MQTT_EVENT_DATA: {
        //     ESP_LOGI(
        //         TAG,
        //         "Received from %.*s: %.*s",
        //         event->topic_len,
        //         event->topic,
        //         event->data_len,
        //         event->data
        //     );
        //     break;
        // }
        case MQTT_EVENT_DATA: {
            // Allocate a null-terminated buffer for the payload
            char *payload = malloc(event->data_len + 1);
            if (payload != NULL) {
                memcpy(payload, event->data, event->data_len);
                payload[event->data_len] = '\0';

                // 1. Extract room name (e.g., "Room 2")
                char room_name[32] = "Unknown Room";
                char *room_ptr = strstr(payload, "\"room\":\"");
                if (room_ptr) {
                    room_ptr += 8; // Move past '"room":"'
                    char *end_ptr = strchr(room_ptr, '"');
                    if (end_ptr) {
                        int len = end_ptr - room_ptr;
                        if (len < sizeof(room_name)) {
                            memcpy(room_name, room_ptr, len);
                            room_name[len] = '\0';
                        }
                    }
                }

                // 2. Extract occupancy value
                int occupancy = 0;
                char *occ_ptr = strstr(payload, "\"occupancy\":");
                if (occ_ptr) {
                    occupancy = atoi(occ_ptr + 12); // Move past '"occupancy":' and convert
                }

                // 3. Check light status (true/false)
                bool light_on = (strstr(payload, "\"light\":true") != NULL);
                const char *light_status = light_on ? "ON" : "OFF";

                ESP_LOGI(TAG, "Peer status from %s: occupancy: %d Lights: %s",
                         room_name,
                         occupancy,
                         light_status);

                free(payload);
            }
            break;
        }
        case MQTT_EVENT_ERROR: {
            if (event != NULL && event->error_handle != NULL) {
                esp_mqtt_error_codes_t *error = event->error_handle;
                ESP_LOGE(
                    TAG,
                    "MQTT error (type=%d, esp_err=%s, socket_errno=%d, broker_code=%d)",
                    error->error_type,
                    esp_err_to_name(error->esp_tls_last_esp_err),
                    error->esp_transport_sock_errno,
                    error->connect_return_code
                );
            } else {
                ESP_LOGE(TAG, "MQTT connection error (no error details)");
            }
            break;
        }

        default:
            break;
    }
}


void start_mqtt(void)
{
    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read station MAC address: %s", esp_err_to_name(err));
        return;
    }

    snprintf(
        mqtt_client_id,
        sizeof(mqtt_client_id),
        "room2_%02x%02x%02x%02x%02x%02x",
        (unsigned int)mac[0],
        (unsigned int)mac[1],
        (unsigned int)mac[2],
        (unsigned int)mac[3],
        (unsigned int)mac[4],
        (unsigned int)mac[5]
    );

    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = MQTT_URI,
        .credentials.client_id = mqtt_client_id
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client for %s", MQTT_URI);
        return;
    }

    err = esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return;
    }

    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return;
    }

    ESP_LOGI(TAG, "Starting MQTT client %s for %s", mqtt_client_id, MQTT_URI);
}