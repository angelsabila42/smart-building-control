#include <stdio.h>
#include "sd_card.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "serv.h"
#include "rgb.h" 
#include "access_control.h"

void rfid(void);
static const char *TAG = "SD_CARD";

void app_main() {
    rgb_init();
    serv_init();
    
    rfid();
      esp_err_t sd_result = sd_card_init();
    if (sd_result == ESP_OK) {
        log_event("SYSTEM", "ESP-A started");
    } else {
        ESP_LOGW(TAG, "SD card initialization failed: %s",
                 esp_err_to_name(sd_result));
    }
    //Testing
   
}