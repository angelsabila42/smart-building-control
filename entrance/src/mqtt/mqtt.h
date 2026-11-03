#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

void mqtt_start(void);
bool mqtt_wait_for_connection(TickType_t timeout_ticks);
void mqtt_publish_card_scan(uint64_t uid, const char *reader, bool authorized);
void mqtt_publish_building_occupancy(int occupancy);
bool mqtt_get_room_occupancy(int room_number, int *occupancy);
bool mqtt_get_room_light(int room);

#endif