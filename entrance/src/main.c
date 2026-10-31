#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "Serv/serv.h"
#include "Rgb/rgb.h" 
#include "Accesscontrol/access_control.h"
#include "Sdcard/sd_card.h"

void rfid(void);

static const char *TAG = "MAIN";

void app_main() {
    rgb_init();
    serv_init();
    
    rfid();
    esp_err_t ret = sd_card_init();
    if(ret != ESP_OK){
        ESP_LOGE(TAG, "SD card initialization failed");
    }
    log_event("SYSTEM", "Entrance system started");
    //Testing
   
}