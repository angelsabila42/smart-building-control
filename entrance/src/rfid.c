#include <stdint.h>
#include <inttypes.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_log.h"
#include "esp_err.h"

#include "rc522.h"
#include "Rgb/rgb.h"
#include "Serv/serv.h"
#include "Sdcard/sd_card.h"
#include "Passage/passage.h"
#include "Accesscontrol/access_control.h"
#include "Alarm/alarm.h"

//SPI communication pins
#define RFID_MISO GPIO_NUM_37
#define RFID_MOSI GPIO_NUM_35
#define RFID_SCK  GPIO_NUM_36

//Chip Select pins
#define RFID_CS_ENTRY GPIO_NUM_10
#define RFID_CS_EXIT GPIO_NUM_4

static const char *TAG = "RFID";

static rc522_handle_t entry_scanner = NULL;
static rc522_handle_t exit_scanner = NULL;

typedef enum {
    RFID_ENTRY,
    RFID_EXIT
}rfid_reader_id_t;


static rfid_reader_id_t exit_id = RFID_EXIT;
static rfid_reader_id_t entry_id = RFID_ENTRY;


//Manually configuring the bus
static esp_err_t initialize_spi_bus(void)
{
    spi_bus_config_t config = {
    .miso_io_num = 37,
    .mosi_io_num = 35,
    .sclk_io_num = 36,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &config, SPI_DMA_CH_AUTO);
    if(ret != ESP_OK){
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static void rfid_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data){

    rfid_reader_id_t reader_id = *(rfid_reader_id_t *)arg;
    //Does not create an event structure or guarantee that the pointer is valid.
    rc522_event_data_t *data = (rc522_event_data_t *)event_data;
    
    if(event_id == RC522_EVENT_TAG_SCANNED && data != NULL){
        rc522_tag_t *tag = (rc522_tag_t*)data->ptr;

        if(tag != NULL){
            ESP_LOGI(TAG, "Card scanned! Serial number: %" PRIu64, tag->serial_number);

            /* Emergency alarm — open all doors regardless of user or schedule */
            if (alarm_is_active()) {
                ESP_LOGW(TAG, "ALARM ACTIVE: granting emergency access");
                entry_open();
                exit_open();
                return;
            }

            access_result_t result = access_control_check(tag->serial_number);
            
            char uid_str[21];
            snprintf(uid_str, sizeof(uid_str), "%" PRIu64, tag->serial_number);

            switch (reader_id)
            {
            case RFID_ENTRY:
                if (result == ACCESS_GRANTED){
                    ESP_LOGI("ACCESS", "Entry Access Granted");
                    log_event("ENTRY_GRANTED", uid_str);
                    passage_set_pending_entry(tag->serial_number);

                    rgb_entry_granted();
                    entry_open();
            }else{
                    ESP_LOGI("ACCESS", "Entry Access Denied");
                    log_event("ENTRY_DENIED", uid_str);
                    rgb_entry_denied();
                    entry_close();
            }
            break;
            
            case RFID_EXIT:
                if (result == ACCESS_GRANTED){
                    ESP_LOGI("ACCESS", "Exit Access Granted");
                    log_event("EXIT_GRANTED", uid_str);
                    passage_set_pending_exit(tag->serial_number);

                    rgb_exit_granted();
                    exit_open();
                }else{
                    ESP_LOGI("ACCESS", "Exit Access Denied");
                    log_event("EXIT_DENIED", uid_str);

                    rgb_exit_denied();
                    exit_close();

                }
                break;
                
            default:
                ESP_LOGE(TAG, "Unknown RFID reader identifier");
                break;
            }
        }
    }

}

static esp_err_t configure_reader(gpio_num_t cs_pin, rc522_handle_t *scanner, rfid_reader_id_t *reader_id){
    rc522_config_t config = {
        .scan_interval_ms = 50,
        .spi.host = SPI2_HOST,
        .spi.miso_gpio = RFID_MISO,
        .spi.mosi_gpio = RFID_MOSI,
        .spi.sck_gpio = RFID_SCK,
        .spi.sda_gpio = cs_pin,
        .spi.clock_speed_hz = 1000000,
        .spi.bus_is_initialized = true,
    };

    esp_err_t err = rc522_create(&config, scanner);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Reader Creation Failed %s", esp_err_to_name(err));
        return err;
    }

    //Register Events
    err = rc522_register_events(
        *scanner,
        RC522_EVENT_ANY,
        rfid_handler,
        reader_id
    );
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to register events %s", esp_err_to_name(err));
        return err;
    }

    //Start
    err = rc522_start(*scanner);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to start reader %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;

}


void rfid(void)
{
    ESP_LOGI(TAG, "Starting RFID reader...");

    esp_err_t err = initialize_spi_bus();
    if (err != ESP_OK){
        ESP_LOGE(TAG, "SPI initialization failed. Stopping RFID setup.");
        return;
    }

    esp_err_t err_ent = configure_reader(RFID_CS_ENTRY, &entry_scanner, &entry_id);
    if (err_ent != ESP_OK){
        ESP_LOGE(TAG, "Entry reader creation failed.");
    }
    

    esp_err_t err_ex = configure_reader(RFID_CS_EXIT, &exit_scanner, &exit_id);
    if (err_ex != ESP_OK){
        ESP_LOGE(TAG, "Exit reader creation failed.");
    }

    if(err_ex != ESP_OK && err_ent != ESP_OK){
        ESP_LOGE(TAG, "Creation of entry and exit readers failed.");
    }

    ESP_LOGI(TAG, "RFID reader started. Present a card.");
}
