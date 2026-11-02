
#include <stdio.h>
#include <inttypes.h>

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_err.h"

#include "Sdcard/sd_card.h"
#include "Serv/serv.h"
#include "Passage/passage.h"
#include "Accesscontrol/access_control.h"

#define ENTRY_BUTTON GPIO_NUM_1
#define EXIT_BUTTON GPIO_NUM_13

static TimerHandle_t entry_pending_timer = NULL;
static TimerHandle_t exit_pending_timer = NULL;

static uint64_t pending_entry_uid = 0;
static uint64_t pending_exit_uid = 0;

static volatile bool entry_button_pressed = false;
static volatile bool exit_button_pressed = false;

static volatile bool entry_authorization_expired = false;
static volatile bool exit_authorization_expired = false;
static const char *TAG = "BUTTON";

static void button_isr_handler(void *arg){
    gpio_num_t button = (gpio_num_t) arg;
    if(button == ENTRY_BUTTON){
        entry_button_pressed = true;
    }else if(button == EXIT_BUTTON){
        exit_button_pressed = true;
    }
}

void configure_button(){

    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << ENTRY_BUTTON) | (1ULL << EXIT_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    ESP_ERROR_CHECK(gpio_config(&button_config));
    gpio_isr_handler_add(ENTRY_BUTTON, button_isr_handler, (void *) ENTRY_BUTTON);
    gpio_isr_handler_add(EXIT_BUTTON, button_isr_handler, (void *) EXIT_BUTTON);

    ESP_LOGI(TAG, "Buttons configured on GPIO %d and GPIO %d", ENTRY_BUTTON, EXIT_BUTTON);
}

void passage_set_pending_entry(uint64_t uid){
    if(pending_entry_uid == 0){
        pending_entry_uid = uid;
        xTimerStart(entry_pending_timer, 0);
    }else{
        ESP_LOGW(TAG, "Entry lane already has a pending user");
    }
}

void passage_set_pending_exit(uint64_t uid){
    if(pending_exit_uid == 0){
        pending_exit_uid = uid;
        xTimerStart(exit_pending_timer, 0);
    }else{
        ESP_LOGW(TAG, "Exit lane already has a pending user");
    }
}

void confirm_entry(){
    if(pending_entry_uid == 0){
        return;
    }

    char uid_str[21];
    snprintf(uid_str, sizeof(uid_str), "%" PRIu64, pending_entry_uid);
    if(occupancy_update(pending_entry_uid, true)){
        log_event("ENTRY_CONFIRMED", uid_str);

        xTimerStop(entry_pending_timer, 0);

        pending_entry_uid = 0;
    }
}


void confirm_exit(){
    if(pending_exit_uid == 0){
        return;
    }

    char uid_str[21];
    snprintf(uid_str, sizeof(uid_str), "%" PRIu64, pending_exit_uid);
    if(occupancy_update(pending_exit_uid, false)){
        log_event("EXIT_CONFIRMED", uid_str);

        xTimerStop(exit_pending_timer, 0);

        pending_exit_uid = 0;
    }
}

void passage_task(void *params){
    while(1){
        if(entry_authorization_expired){
            entry_authorization_expired = false;
            entry_close();

            if(pending_entry_uid != 0){
        
                char uid_str[21];
                snprintf(uid_str, sizeof(uid_str), "%" PRIu64, pending_entry_uid);
                log_event("ENTRY_EXPIRED", uid_str);

                pending_entry_uid = 0;
            }
        }

        if(exit_authorization_expired){
            exit_authorization_expired = false;
            entry_close();

            if(pending_exit_uid != 0){
        
                char uid_str[21];
                snprintf(uid_str, sizeof(uid_str), "%" PRIu64, pending_exit_uid);
                log_event("EXIT_EXPIRED", uid_str);

                pending_exit_uid = 0;
            }
        }

        if (entry_button_pressed){
            entry_button_pressed = false;
            confirm_entry();
        }
        
        if(exit_button_pressed){
            exit_button_pressed = false;
            confirm_exit();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


//Exit Pending Timer
static void entry_pending_timer_callback(TimerHandle_t timer){
    entry_authorization_expired = true;
}

static void configure_entry_pending_timer()
{
    entry_pending_timer = xTimerCreate(
        "Entry Pending Timer",
        pdMS_TO_TICKS(2000),
        pdFALSE,
        NULL,
        entry_pending_timer_callback
        );
    

    if(entry_pending_timer == NULL){
        ESP_LOGE(TAG, "Failed to create entry pending timer");
    }
}


//Exit Pending Timer
static void exit_pending_timer_callback(TimerHandle_t timer){
    exit_authorization_expired = true;
}

static void configure_exit_pending_timer()
{
    exit_pending_timer = xTimerCreate(
        "Exit Pending Timer",
        pdMS_TO_TICKS(2000),
        pdFALSE,
        NULL,
        exit_pending_timer_callback
        );
    

    if(exit_pending_timer == NULL){
        ESP_LOGE(TAG, "Failed to create exit pending timer");
    }
}

void passage_task_init(){
    BaseType_t result = xTaskCreate(passage_task, "Passage Task", 4096, NULL, 1, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create passage task");
    }
}

void button_init(void){
    gpio_install_isr_service(0);
    configure_button();

    configure_entry_pending_timer();
    configure_exit_pending_timer();

}