#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"

void wifi_init(void);
bool wifi_wait_for_connection(TickType_t timeout_ticks);

#endif