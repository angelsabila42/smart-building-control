#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_USERS 20

typedef enum {
    ACCESS_GRANTED = 0,
    ACCESS_DENIED  = 1
} access_result_t;

typedef struct {
    uint32_t id;
    uint64_t rfid_uid;
    bool     authorized;
    bool     inside;
} user_t;

access_result_t access_control_check(uint64_t uid);
bool            occupancy_update(uint64_t uid, bool entering);
int             occupancy_get_current(void);

bool            user_add(uint32_t id, uint64_t rfid_uid);
bool            user_remove(uint64_t rfid_uid);
int             user_list(user_t *out_buf, int buf_len);

#endif