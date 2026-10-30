#include <stdio.h>

#include "management/management_app.h"

void rfid(void);

void app_main(void)
{
    /* NVS, access policy, administrator session and the UART menu. */
    management_app_start();

    /* RFID reader and door-event handling. */
    rfid();
}
