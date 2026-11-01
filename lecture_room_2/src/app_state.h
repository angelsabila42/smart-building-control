#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "mqtt_client.h" 

#define PIR1 GPIO_NUM_2
#define PIR2 GPIO_NUM_3
#define LIGHT GPIO_NUM_4

#define SEQUENCE_TIMEOUT_US 3000000LL

extern int occupancy;
extern int first_sensor;
extern int64_t first_time;

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

//#define MQTT_URI "mqtt://test.mosquitto.org"
#define MQTT_URI "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC "cocis/room1/status"

#define TAG "ROOM2"

extern esp_mqtt_client_handle_t mqtt_client;
extern bool mqtt_connected;