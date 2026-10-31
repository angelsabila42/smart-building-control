#include <stdint.h>
#include "serv.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#define SERVO1 GPIO_NUM_2
#define SERVO2 GPIO_NUM_14

#define SERVO_MIN_PULSE_WIDTH 500 // 1ms pulse width (0 degrees)
#define SERVO_MAX_PULSE_WIDTH 1500

#define FREQ_HZ 50

static const char *TAG = "SERVO";

static void configure_servo_timer(void)
{
    // Configure the PWM timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = FREQ_HZ,
        .clk_cfg =  LEDC_USE_XTAL_CLK
    };

    esp_err_t err = ledc_timer_config(&ledc_timer);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Servo timer configuration failed: %s", esp_err_to_name(err));
    }
}

static void configure_servo_channel(gpio_num_t servo_pin,
                             ledc_channel_t channel_num)
{
    // Configure the PWM channel
    ledc_channel_config_t servo_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel_num,
        .timer_sel = LEDC_TIMER_1,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = servo_pin,
        .duty = 0, // Start with 0% duty cycle
        .hpoint = 0
    };

    ledc_channel_config(&servo_channel);
}

static void servo_set_position(ledc_channel_t channel, uint32_t pulse_width_us)
{
    if(pulse_width_us > SERVO_MAX_PULSE_WIDTH || pulse_width_us < SERVO_MIN_PULSE_WIDTH){
        ESP_LOGE(TAG, "Pulse Width must be between 500 and 1500");
        return;
    }

    //Convert pulse width into duty value.
    uint32_t period = 1000000 / FREQ_HZ ;
    uint32_t levels = 1U << 10; 
    //ESP-IDF's ledc_set_duty() expects the duty argument as uint32_t
    uint32_t duty_value = (pulse_width_us * levels + period / 2) / period;
    
    //Set and update the LEDC duty.
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty_value);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

void entry_open(void)
{
    servo_set_position(LEDC_CHANNEL_6, SERVO_MAX_PULSE_WIDTH);
}

void entry_close(void)
{
    servo_set_position(LEDC_CHANNEL_6, SERVO_MIN_PULSE_WIDTH);
}

void exit_open(void)
{
    servo_set_position(LEDC_CHANNEL_7, SERVO_MAX_PULSE_WIDTH);
}

void exit_close(void)
{
    servo_set_position(LEDC_CHANNEL_7, SERVO_MIN_PULSE_WIDTH);
}

void serv_init(){
    //Configure timer
    configure_servo_timer();

    //Configure PWM pins
    configure_servo_channel(SERVO1, LEDC_CHANNEL_6);
    configure_servo_channel(SERVO2, LEDC_CHANNEL_7);

    //Initialise with both doors closed
    entry_close();
    exit_close();

}