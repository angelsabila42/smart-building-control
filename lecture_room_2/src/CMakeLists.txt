idf_component_register(
    SRCS "main.c" "wifi/wifi.c" "mqtt/mqtt.c" "controller/controller.c" "pins/pins.c" "mqtt.c" "wifi.c"
    INCLUDE_DIRS "." "wifi" "mqtt" "controller" "pins"
    REQUIRES mqtt esp_wifi esp_event esp_netif nvs_flash esp_driver_gpio esp_timer log
)