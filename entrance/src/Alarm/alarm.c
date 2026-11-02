#include "alarm.h"
#include "Serv/serv.h"
#include "Sdcard/sd_card.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ALARM_GPIO       GPIO_NUM_3
#define ALARM_RESET_GPIO GPIO_NUM_6

static const char *TAG = "ALARM";
static volatile bool alarm_active = false;

static void IRAM_ATTR alarm_isr_handler(void *arg)
{
    alarm_active = true;
}

static void IRAM_ATTR alarm_reset_isr_handler(void *arg)
{
    alarm_active = false;
}

bool alarm_is_active(void)
{
    return alarm_active;
}

void alarm_set(bool active)
{
    alarm_active = active;
    if (active) {
        ESP_LOGW(TAG, "Alarm ACTIVATED — all doors opening");
        entry_open();
        exit_open();
        log_event("ALARM", "ACTIVATED");
    } else {
        ESP_LOGI(TAG, "Alarm CLEARED — normal access resumed");
        entry_close();
        exit_close();
        log_event("ALARM", "CLEARED");
    }
}

void alarm_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ALARM_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io_conf);

    /* Reset button — GPIO 6, active high, clears the alarm */
    io_conf.pin_bit_mask = (1ULL << ALARM_RESET_GPIO);
    io_conf.intr_type    = GPIO_INTR_POSEDGE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(ALARM_GPIO, alarm_isr_handler, NULL);
    gpio_isr_handler_add(ALARM_RESET_GPIO, alarm_reset_isr_handler, NULL);
    ESP_LOGI(TAG, "Alarm input initialised on GPIO %d", ALARM_GPIO);
}
