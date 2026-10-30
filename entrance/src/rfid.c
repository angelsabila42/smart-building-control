#include "driver/spi_master.h"

#include <stdbool.h>

#include "driver/gpio.h"

#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "management/management_app.h"

/*
rc522 start()
Checks the handle: makes sure the reader pointer is valid.
Checks the state: confirms the reader is allowed to start.
Handles resuming: if the reader was paused, it resumes polling without reinitializing the chip.
Initializes the hardware: resets the chip, tests read/write communication, and initializes the proximity-coupling device (PCD).
Enables polling: sets the reader state to RC522_STATE_POLLING, allowing the polling task you examined earlier to detect cards.

PICC stands for Proximity Integrated Circuit Card. It refers to the RFID card or tag that you present to the RC522 reader.
*/

#define RST_PIN GPIO_NUM_9   
#define SS_PIN GPIO_NUM_10  
#define SCK_PIN GPIO_NUM_36  
#define MOSI_PIN GPIO_NUM_35  
#define MISO_PIN GPIO_NUM_37

static const char *RFID = "RFID";

rc522_driver_handle_t driver = NULL;
rc522_handle_t reader = NULL;

spi_bus_config_t bus_config = {0};
rc522_config_t reader_config ={0};

void configure_bus(){

    bus_config.sclk_io_num = SCK_PIN;
    bus_config.mosi_io_num =  MOSI_PIN;
    bus_config.miso_io_num =  MISO_PIN;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;

}

void configure_driver(){

    rc522_spi_config_t spi_config = {0};
    spi_config.bus_config = &bus_config;
    spi_config.host_id = SPI2_HOST;
    spi_config.rst_io_num = RST_PIN;
    //Configure the chip-select pin
    spi_config.dev_config.spics_io_num = SS_PIN;
    //SPI communication settings
    spi_config.dev_config.clock_speed_hz = 1000000;
    spi_config.dev_config.mode = 0; //RC522 commonly communicates using SPI mode 0'
    spi_config.dev_config.queue_size = 1;
    spi_config.dma_chan = SPI_DMA_CH_AUTO;

    esp_err_t err = rc522_spi_create(&spi_config, &driver);
    if(err != ESP_OK){
        ESP_LOGE(RFID, "Failed to create SPI driver %s", esp_err_to_name(err));
        return;
    }

    err = rc522_driver_install(driver);
    if(err != ESP_OK){
        ESP_LOGE(RFID, "Failed to install SPI driver %s", esp_err_to_name(err));
        return;
    }

}

static void card_event_handler(void *arg, esp_event_base_t event_t, int32_t id, void *event_data){
    //Does not create an event structure or guarantee that the pointer is valid.
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)event_data;
    
    if(event == NULL || event->picc == NULL){
        return;
    }

    if(event->picc->state == RC522_PICC_STATE_ACTIVE || event->picc->state == RC522_PICC_STATE_ACTIVE_H){
        //This creates a character array to hold the converted UID. The UID is stored as bytes in event->picc->uid, 
        //but we want to display it as readable hexadecimal text, such as A1:B2:C3:D4.

        char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX];
        esp_err_t err = rc522_picc_uid_to_str(
            &event->picc->uid,
            uid_str,
            sizeof(uid_str)
        );

        if(err != ESP_OK){
            ESP_LOGE(RFID, "Failed to convert card UID: %s", esp_err_to_name(err));
            return;
        }

        /*
         * Management interface integration: apply the configured access
         * schedule and record the decision.  The current entrance has a single
         * reader, so presentations are treated as entry attempts.  When the
         * exit reader is added, call management_access_check(uid, 'O') there.
         */
        const bool granted = management_access_check(uid_str, 'I');

        ESP_LOGI(RFID, "Card UID: %s - access %s", uid_str, granted ? "GRANTED" : "DENIED");
    }

}

void configure_reader(){
    
    reader_config.driver = driver;
    reader_config.poll_interval_ms = 1000;
    reader_config.task_stack_size = 4096;
    reader_config.task_priority = 5;

    esp_err_t err = rc522_create(&reader_config, &reader);
    if(err != ESP_OK){
        ESP_LOGE(RFID, "Failed to create SPI driver %s", esp_err_to_name(err));
        return;
    }

    //Register Events
    err = rc522_register_events(
        reader,
        RC522_EVENT_PICC_STATE_CHANGED,
        card_event_handler,
        NULL
    );
    if(err != ESP_OK){
        ESP_LOGE(RFID, "Failed to register events %s", esp_err_to_name(err));
        return;
    }

    //Start
    err = rc522_start(reader);
    if(err != ESP_OK){
        ESP_LOGE(RFID, "Failed to start reader %s", esp_err_to_name(err));
        return;
    }

}


void rfid(){
    ESP_LOGI(RFID, "RFID, Entance Starting...");
    configure_bus();
    configure_driver();
    configure_reader();    
}