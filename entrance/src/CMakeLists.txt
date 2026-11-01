# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS
        "main.c"
        "rfid.c"
        "Accesscontrol/access_control.c"
        "Rgb/rgb.c"
        "Serv/serv.c"
        "Sdcard/sd_card.c"
        "Passage/passage.c"
        "wifi/wifi.c"
        "mqtt/mqtt.c"

    INCLUDE_DIRS
        "."
        "Rgb"
        "Serv"
        "Sdcard"
        "Accesscontrol"
        "Passage"
        "wifi"
        "mqtt"
        esp_wifi 
        esp_event 
        nvs_flash 
        esp_netif
    PRIV_REQUIRES 
        esp_driver_gpio 
        esp_driver_spi 
        esp_driver_ledc 
        esp_driver_sdspi 
        rc522
        fatfs
        mqtt
        esp_event

)
