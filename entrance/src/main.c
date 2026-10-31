#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "serv.h"
#include "rgb.h" 
#include "access_control.h"
#include "sd_card.h"
#include "wifi.h"

void rfid(void);

static const char *TAG = "MAIN";

void app_main(void) {
    esp_err_t sd_err = sd_card_init();
    if (sd_err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "SD-card logging is unavailable: %s",
            esp_err_to_name(sd_err)
        );
    }

    wifi_init();
    rgb_init();
    serv_init();
    
    rfid();
    //Testing
   
}