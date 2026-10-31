#include "app_state.h"
#include "controller.h"

int occupancy = 0;
int first_sensor = 0;
int64_t first_time = 0;

static void publish_room_status(int count, bool light_on)
{
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT disconnected; status not published");
        return;
    }

    char message[100];

    snprintf(
        message,
        sizeof(message),
        "{\"room\":\"Room 1\",\"occupancy\":%d,\"light\":%s}",
        count,
        light_on ? "true" : "false"
    );

    int message_id = esp_mqtt_client_publish(
        mqtt_client,
        MQTT_TOPIC,
        message,
        0,
        1,
        0
    );

    ESP_LOGI(TAG, "Published message ID: %d", message_id);
    ESP_LOGI(TAG, "Message: %s", message);
}

void control(){
        int pir1 = gpio_get_level(PIR1);
        int pir2 = gpio_get_level(PIR2);
        int64_t now = esp_timer_get_time();


        if (first_sensor != 0 &&
            now - first_time > SEQUENCE_TIMEOUT_US) {
            first_sensor = 0;
        }

        if (pir1 && !pir2 && first_sensor == 0) {
            first_sensor = 1;
            first_time = now;
        }

        // PIR 2 activates first
        else if (pir2 && !pir1 && first_sensor == 0) {
            first_sensor = 2;
            first_time = now;
        }

        // PIR 1 followed by PIR 2: entering
        else if (pir1 && pir2 && first_sensor == 1) {
            occupancy++;
            gpio_set_level(LIGHT, occupancy > 0);
            publish_room_status(occupancy, occupancy > 0);
            first_sensor = 0;

            printf("Entry detected. Occupancy: %d\n", occupancy);
        }


        else if (pir1 && pir2 && first_sensor == 2) {
            if (occupancy > 0) {
                occupancy--;

                gpio_set_level(LIGHT, occupancy > 0);
                publish_room_status(occupancy, occupancy > 0);
            }

            first_sensor = 0;

            printf("Exit detected. Occupancy: %d\n", occupancy);
        }

        // Light follows estimated occupancy
        gpio_set_level(LIGHT, occupancy > 0 ? 1 : 0);

        vTaskDelay(pdMS_TO_TICKS(50));
}