#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stdbool.h>

/* 0=Monday ... 6=Sunday */
typedef struct {
    int  open_hour;   /* 0-23 */
    int  close_hour;  /* 0-23, must be > open_hour */
    bool closed;      /* true = building closed all day */
} day_schedule_t;

void            schedule_init(void);
day_schedule_t  schedule_get_day(int day);
bool            schedule_set_day(int day, int open_hour, int close_hour, bool closed);
bool            schedule_is_open(int day, int current_hour);

#endif
