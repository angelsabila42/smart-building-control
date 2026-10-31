#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "Accesscontrol/access_control.h"
#include "esp_log.h"

//Define user structure
typedef struct {
    uint32_t id;
    uint64_t rfid_uid;
    bool authorized;
    bool inside;
}user_t;

static int occupancy;

//Create the student records
static user_t users [] = {
    {230000, 17247175169ULL, true, false},
    {230001, 262913, true, false},
    {230002, 13399893, true, false},
    {230003, 4469521, true, false},
    {230004, 52394, false, false}
};

static const size_t num_records = sizeof(users) / sizeof(users[0]);



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
            //check occupancy and state
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