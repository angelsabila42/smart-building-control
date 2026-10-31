#include <stdio.h>
#include "Sdcard/sd_card.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "Serv/serv.h"
#include "Rgb/rgb.h"
#include "Accesscontrol/access_control.h"
#include "Sdcard/sd_card.h"

#include "management/management_app.h"

void rfid(void);

static const char *TAG = "MAIN";

/* Bridges the management audit hook to the SD-card event log. */
static void sd_audit_sink(const char *event, const char *details)
{
    (void)log_event(event, details);
}

void app_main(void)
{
    /* NVS, access policy, administrator session and the UART menu. */
    management_app_start();

    rgb_init();
    serv_init();

    rfid();

    esp_err_t sd_result = sd_card_init();
    if (sd_result == ESP_OK) {
        log_event(TAG, "Entrance system started");
        management_app_set_storage(true);
        management_app_set_audit_hook(sd_audit_sink);
    } else {
        ESP_LOGW(TAG, "SD card initialization failed: %s",
        esp_err_to_name(sd_result));
    }

}
