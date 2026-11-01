#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "Serv/serv.h"
#include "Rgb/rgb.h" 
#include "Passage/passage.h"
#include "Accesscontrol/access_control.h"
#include "Sdcard/sd_card.h"
#include "wifi/wifi.h"

void rfid(void);

static const char *TAG = "MAIN";

void app_main() {
    wifi_init();
    rgb_init();
    serv_init();

    rfid();

    esp_err_t sd_result = sd_card_init();

    button_init();
    passage_task_init();
    

    if (sd_result == ESP_OK) {
        log_event(TAG, "Entrance system started");
    } else {
        ESP_LOGW(TAG, "SD card initialization failed: %s",
                 esp_err_to_name(sd_result));
    }
   
}