idf_component_register(
    INCLUDE_DIRS .
    SRCS rc522.c
    REQUIRES esp_event driver esp_driver_spi
)
