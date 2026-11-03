#include "Alarm/alarm.h"
#include "Serv/serv.h"
#include "Sdcard/sd_card.h"
#include "Rgb/rgb.h"

#include "driver/gpio.h"
#include "Rgb/rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ALARM_GPIO       GPIO_NUM_3
#define ALARM_RESET_GPIO GPIO_NUM_6
#define NOTIFY_TRIGGER   0x01
#define NOTIFY_RESET     0x02

static const char *TAG = "ALARM";
static volatile bool alarm_active = false;
static TaskHandle_t alarm_task_handle = NULL;

static void IRAM_ATTR alarm_isr_handler(void *arg)
{
    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(alarm_task_handle, NOTIFY_TRIGGER, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

static void IRAM_ATTR alarm_reset_isr_handler(void *arg)
{
    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(alarm_task_handle, NOTIFY_RESET, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

static void alarm_task(void *arg)
{
    uint32_t bits;
    while (1) {
        xTaskNotifyWait(0, 0xFFFFFFFF, &bits, portMAX_DELAY);
        if (bits & NOTIFY_TRIGGER) {
            alarm_set(true);
        }
        if (bits & NOTIFY_RESET) {
            alarm_set(false);
        }
    }
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
        // Activate emergency release for all doors
        serv_set_emergency_release(true);
        entry_open();
        exit_open();

        rgb_entry_denied();
        rgb_exit_denied();
        
        log_event("ALARM", "ACTIVATED");
    } else {
        ESP_LOGI(TAG, "Alarm CLEARED — normal access resumed");
        // Deactivate emergency release for all doors
        serv_set_emergency_release(false);
        entry_close();
        exit_close();
        log_event("ALARM", "CLEARED");
    }
}

void alarm_init(void)
{
    xTaskCreate(alarm_task, "alarm_task", 4096, NULL, 5, &alarm_task_handle);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ALARM_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << ALARM_RESET_GPIO);
    io_conf.intr_type    = GPIO_INTR_POSEDGE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(ALARM_GPIO, alarm_isr_handler, NULL);
    gpio_isr_handler_add(ALARM_RESET_GPIO, alarm_reset_isr_handler, NULL);
    ESP_LOGI(TAG, "Alarm input initialised on GPIO %d", ALARM_GPIO);
}
