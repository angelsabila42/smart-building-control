#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum{
    ACCESS_GRANTED = 0,
    ACCESS_DENIED = 1
} access_result_t;

access_result_t access_control_check(uint64_t uid);
bool occupancy_update(uint64_t uid, bool entering);
int occupancy_get_current(void);
#endif