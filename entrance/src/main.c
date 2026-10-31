#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "serv.h"
#include "rgb.h" 
#include "access_control.h"

void rfid(void);

void app_main() {
    rgb_init();
    serv_init();
    
    rfid();
    //Testing
   
}