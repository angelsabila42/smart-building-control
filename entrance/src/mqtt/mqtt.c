#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mqtt.h"
#include "Sdcard/sd_card.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"

#define MQTT_URI "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC1 "cocis/room1/status"
#define MQTT_TOPIC2 "cocis/room2/status"
#define MQTT_TOPIC2_TELEMETRY "cocis/rooms/room2/occupancy"

static const char *TAG = "ENTRANCE";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static int room1_occupancy = 0;
static int room2_occupancy = 0;
static bool room1_light_on = false;
static bool room2_light_on = false;

static bool parse_room_status(
    const char *payload,
    int *room_number,
    int *occupancy,
    bool *light_on)
{
    char room[16];
    char light_value[8];

    if (sscanf(
            payload,
            " { \"room\" : \"%15[^\"]\" , \"occupancy\" : %d , "
            "\"light\" : %7[^,}] }",
            room,
            occupancy,
            light_value) == 3) {
        if (strcmp(light_value, "true") == 0) {
            *light_on = true;
        } else if (strcmp(light_value, "false") == 0) {
            *light_on = false;
        } else {
            return false;
        }
    } else {
        char occupancy_value[8];
        int light_status;

        if (sscanf(
                payload,
                " { \"room\" : \"%15[^\"]\" , \"occupancy\" : %7[^,] , "
                "\"light_status\" : %d",
                room,
                occupancy_value,
                &light_status) != 3 ||
            strcmp(room, "room2") != 0 ||
            (strcmp(occupancy_value, "true") != 0 &&
             strcmp(occupancy_value, "false") != 0)) {
            return false;
        }

        *occupancy = strcmp(occupancy_value, "true") == 0 ? 1 : 0;
        *light_on = light_status != 0;
    }

    if (strcmp(room, "Room 1") == 0) {
        *room_number = 1;
    } else if (strcmp(room, "Room 2") == 0 || strcmp(room, "room2") == 0) {
        *room_number = 2;
    } else {
        return false;
    }

    return true;
}

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data)
{
    esp_mqtt_event_handle_t event =
        (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            esp_mqtt_client_subscribe(event->client, MQTT_TOPIC2, 1);
            esp_mqtt_client_subscribe(event->client, MQTT_TOPIC1, 1);
            esp_mqtt_client_subscribe(
                event->client,
                MQTT_TOPIC2_TELEMETRY,
                1
            );
            break;

        case MQTT_EVENT_DATA: {
            char payload[128];
            int len = event->data_len;

            if (len >= sizeof(payload)) {
                ESP_LOGW(TAG, "MQTT message too large");
                break;
            }

            memcpy(payload, event->data, len);
            payload[len] = '\0';

            int room_number;
            int occupancy;
            bool light_on;

            if (!parse_room_status(
                    payload,
                    &room_number,
                    &occupancy,
                    &light_on)) {
                ESP_LOGW(TAG, "Could not parse: %s", payload);
                break;
            }

            if (room_number == 1) {
                room1_occupancy = occupancy;
                room1_light_on = light_on;

                ESP_LOGI(
                    TAG,
                    "Room 1 occupancy=%d light=%s",
                    room1_occupancy,
                    room1_light_on ? "ON" : "OFF"
                );

                char details[100];
                snprintf(
                    details,
                    sizeof(details),
                    "Room 1 occupancy=%d light=%s",
                    room1_occupancy,
                    room1_light_on ? "ON" : "OFF"
                );
                log_event("ROOM1_STATUS", details);
            } else {
                room2_occupancy = occupancy;
                room2_light_on = light_on;

                ESP_LOGI(
                    TAG,
                    "Room 2 occupancy=%d light=%s",
                    room2_occupancy,
                    room2_light_on ? "ON" : "OFF"
                );

                char details[100];
                snprintf(
                    details,
                    sizeof(details),
                    "Room 2 occupancy=%d light=%s",
                    room2_occupancy,
                    room2_light_on ? "ON" : "OFF"
                );
                log_event("ROOM2_STATUS", details);
            }
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;

        case MQTT_EVENT_ERROR:
            if (event->error_handle != NULL) {
                const esp_mqtt_error_codes_t *error = event->error_handle;
                ESP_LOGE(
                    TAG,
                    "MQTT error type=%d, socket_errno=%d (%s), "
                    "tls_error=%s, tls_stack_error=0x%x, "
                    "cert_flags=0x%x, broker_return_code=%d",
                    error->error_type,
                    error->esp_transport_sock_errno,
                    strerror(error->esp_transport_sock_errno),
                    esp_err_to_name(error->esp_tls_last_esp_err),
                    error->esp_tls_stack_err,
                    error->esp_tls_cert_verify_flags,
                    error->connect_return_code
                );
            } else {
                ESP_LOGE(TAG, "MQTT error event without error details");
            }
            break;

        default:
            break;
    }
}

void mqtt_start(void)
{
    if (mqtt_client != NULL) {
        return;
    }

    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = MQTT_URI,
        .credentials.client_id = "esp32_entrance_controller"
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );
    esp_mqtt_client_start(mqtt_client);
}