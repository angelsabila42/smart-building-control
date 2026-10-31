# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS "main.c" "rfid.c" "rgb.c" "serv.c" "access_control.c" "Sdcard/sd_card.c" "wifi/wifi.c" "mqtt/mqtt.c"
    INCLUDE_DIRS "." "wifi" "mqtt" "Sdcard"
    REQUIRES esp_wifi esp_event nvs_flash esp_netif
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi rc522 esp_driver_ledc esp_driver_sdspi 
)