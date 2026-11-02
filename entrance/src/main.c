#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "Sdcard/sd_card.h"
#include "Serv/serv.h"
#include "Rgb/rgb.h"
#include "Accesscontrol/access_control.h"
#include "Passage/passage.h"
#include "Schedule/schedule.h"
#include "AdminAuth/admin_auth.h"
#include "SerialMenu/serial_menu.h"
#include "Alarm/alarm.h"
#include "wifi/wifi.h"

void rfid(void);

static const char *TAG = "MAIN";

void app_main(void)
{
    wifi_init();
    rgb_init();
    serv_init();
    schedule_init();
    admin_auth_init();
    alarm_init();

    rfid();

    esp_err_t sd_result = sd_card_init();

    button_init();
    passage_task_init();

    if (sd_result == ESP_OK) {
        log_event(TAG, "Entrance system started");
    } else {
        ESP_LOGW(TAG, "SD card initialization failed: %s", esp_err_to_name(sd_result));
    }

    serial_menu_init();
}
