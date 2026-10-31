/*Common Anode setup*/
#include <stdint.h>
#include "Rgb/rgb.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define RED_LED1 GPIO_NUM_11
#define GREEN_LED1 GPIO_NUM_12
#define BLUE_LED1 GPIO_NUM_17

#define RED_LED2 GPIO_NUM_5
#define GREEN_LED2 GPIO_NUM_15
#define BLUE_LED2 GPIO_NUM_16

const static char *TAG = "RGB";

static void setColor (uint8_t red,
            uint8_t green,
            uint8_t blue,
            ledc_channel_t red_channel_num,
            ledc_channel_t green_channel_num,
            ledc_channel_t blue_channel_num)
{

    ledc_set_duty(LEDC_LOW_SPEED_MODE, red_channel_num, red);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, green_channel_num, green);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, blue_channel_num, blue);
    
    ledc_update_duty(LEDC_LOW_SPEED_MODE,  red_channel_num);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, green_channel_num);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, blue_channel_num);
}

static void configure_rgb_timer(void)
{
    // Configure the PWM timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg =  LEDC_USE_XTAL_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "RGB timer configuration failed: %s", esp_err_to_name(err));
    }
}

static void configure_rgb_pwm(gpio_num_t RED_LED, 
                    gpio_num_t GREEN_LED,
                    gpio_num_t BLUE_LED,
                    ledc_channel_t red_channel_num,
                    ledc_channel_t green_channel_num,
                    ledc_channel_t blue_channel_num){
    

    // Configure the PWM channel
    ledc_channel_config_t red_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = red_channel_num,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = RED_LED,
        .duty = 255, // Start with 0% duty cycle
        .hpoint = 0
    };  

    ledc_channel_config_t green_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = green_channel_num,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = GREEN_LED,
        .duty = 255, // Start with 0% duty cycle
        .hpoint = 0
    };  

    ledc_channel_config_t blue_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = blue_channel_num,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BLUE_LED,
        .duty = 255, // Start with 0% duty cycle
        .hpoint = 0
    };  

    ledc_channel_config(&red_channel);
    ledc_channel_config(&green_channel);
    ledc_channel_config(&blue_channel);

}

void exit_rgb_off(){
    setColor(255,255,255,LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5);
    
}

void entry_rgb_off(){
    setColor(255,255,255,LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2);
}

void rgb_entry_granted(){
    //Green
    setColor(255,0,255,LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2);
}

void rgb_entry_denied(){
    //Red
    setColor(0,255,255,LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2);
}

void rgb_exit_granted(){
    //Green
    setColor(255,0,255,LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5);
}

void rgb_exit_denied(){
    //Red
    setColor(0,255,255,LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5);
}

void rgb_init(void){
    configure_rgb_timer();
    //Configure entry RGB LED
    configure_rgb_pwm(
        RED_LED1,
        GREEN_LED1,
        BLUE_LED1,
        LEDC_CHANNEL_0,
        LEDC_CHANNEL_1,
        LEDC_CHANNEL_2);

    //Configure exit RGB LED
    configure_rgb_pwm(
        RED_LED2,
        GREEN_LED2,
        BLUE_LED2,
        LEDC_CHANNEL_3,
        LEDC_CHANNEL_4,
        LEDC_CHANNEL_5);

        //Turn both off initially
        entry_rgb_off();
        exit_rgb_off();
    
}