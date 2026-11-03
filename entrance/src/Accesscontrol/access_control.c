#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "Accesscontrol/access_control.h"
#include "esp_log.h"

#define USERS_FILE "/sdcard/users.csv"

static int occupancy;

static user_t users[MAX_USERS] = {
    {230000, 17247175169ULL,  true,  false},
    {230001, 262913,          true,  false},
    {230002, 293201977873ULL, true,  false},
    {230003, 4469521,         true,  false},
    {230004, 52394,           false, false}
};

static size_t num_records = 5;


access_result_t access_control_check(uint64_t uid)
{
    for(int i = 0; i < num_records; i++){
        if (users[i].rfid_uid == uid ){
            if (users[i].authorized == 1){
                return ACCESS_GRANTED;
            }else{
                return ACCESS_DENIED;
            }
        }
    }
    return ACCESS_DENIED;
}

bool occupancy_update(uint64_t uid, bool entering){
    for (size_t i = 0; i < num_records; i++) {
        if (users[i].rfid_uid == uid ){
            if(users[i].authorized && !users[i].inside && entering){
                occupancy += 1;
                ESP_LOGI("OCCUPANCY", "Current Occupancy: %d", occupancy);
                users[i].inside = true;
                return true;
            }

            if(users[i].authorized && users[i].inside && !entering){
                occupancy -= 1;
                ESP_LOGI("OCCUPANCY", "Current Occupancy: %d", occupancy);
                users[i].inside = false;
                return true;
            }
        }
    }

    return false;
}

int occupancy_get_current(void)
{
    return occupancy;
}

bool user_add(uint32_t id, uint64_t rfid_uid)
{
    if (num_records >= MAX_USERS) return false;
    for (size_t i = 0; i < num_records; i++) {
        if (users[i].rfid_uid == rfid_uid) return false;
    }
    users[num_records].id         = id;
    users[num_records].rfid_uid   = rfid_uid;
    users[num_records].authorized = true;
    users[num_records].inside     = false;
    num_records++;
    users_save();
    return true;
}

bool user_remove(uint64_t rfid_uid)
{
    for (size_t i = 0; i < num_records; i++) {
        if (users[i].rfid_uid == rfid_uid) {
            users[i] = users[num_records - 1];
            num_records--;
            users_save();
            return true;
        }
    }
    return false;
}

void users_save(void)
{
    FILE *f = fopen(USERS_FILE, "w");
    if (f == NULL) {
        ESP_LOGE("ACCESS", "Could not open users file for writing");
        return;
    }
    for (size_t i = 0; i < num_records; i++) {
        fprintf(f, "%" PRIu32 ",%" PRIu64 ",%d\n",
                users[i].id, users[i].rfid_uid, users[i].authorized ? 1 : 0);
    }
    fclose(f);
    ESP_LOGI("ACCESS", "User table saved (%d records)", (int)num_records);
}

void users_load(void)
{
    FILE *f = fopen(USERS_FILE, "r");
    if (f == NULL) {
        ESP_LOGW("ACCESS", "No users file found, using defaults");
        return;
    }
    size_t count = 0;
    uint32_t id;
    uint64_t uid;
    int auth;
    while (count < MAX_USERS &&
           fscanf(f, "%" SCNu32 ",%" SCNu64 ",%d\n", &id, &uid, &auth) == 3) {
        users[count].id         = id;
        users[count].rfid_uid   = uid;
        users[count].authorized = auth != 0;
        users[count].inside     = false;
        count++;
    }
    fclose(f);
    num_records = count;
    ESP_LOGI("ACCESS", "Loaded %d users from SD card", (int)num_records);
}

int user_list(user_t *out_buf, int buf_len)
{
    int count = (int)num_records < buf_len ? (int)num_records : buf_len;
    memcpy(out_buf, users, count * sizeof(user_t));
    return count;
}
