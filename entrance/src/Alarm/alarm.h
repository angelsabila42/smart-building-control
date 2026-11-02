#ifndef ALARM_H
#define ALARM_H

#include <stdbool.h>

void alarm_init(void);
bool alarm_is_active(void);
void alarm_set(bool active);

#endif
