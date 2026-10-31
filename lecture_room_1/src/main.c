#include "wifi.h"
#include "mqtt.h"
#include "pins.h"
#include "controller.h"


void app_main(void)
{
    init_pins();
    start_wifi();
    while (1)
    {
        control();
    }
    
}