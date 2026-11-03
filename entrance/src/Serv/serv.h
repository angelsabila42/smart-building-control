#ifndef SERV_H
#define SERV_H

#include <stdbool.h>

void serv_init(void);
void serv_set_emergency_release(bool active);

void entry_open(void);
void entry_close(void);

void exit_open(void);
void exit_close(void);

#endif