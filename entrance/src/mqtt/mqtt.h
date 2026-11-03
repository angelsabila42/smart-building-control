#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>
#include <stdint.h>

void mqtt_start(void);
void mqtt_publish_card_scan(uint64_t uid, const char *reader, bool authorized);
void mqtt_publish_building_occupancy(int occupancy);

int  mqtt_get_room_occupancy(int room);
bool mqtt_get_room_light(int room);

#endif