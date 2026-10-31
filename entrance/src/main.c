#include <stdio.h>
#include "sd_card.h"

void rfid(void);
static const char *TAG = "SD_CARD";

void app_main() {
    
    rfid();
      esp_err_t sd_result = sd_card_init();
    if (sd_result == ESP_OK) {
        log_event("SYSTEM", "ESP-A started");
    } else {
        ESP_LOGW(TAG, "SD card initialization failed: %s",
                 esp_err_to_name(sd_result));
    }
}