#ifndef MANAGEMENT_STORE_H
#define MANAGEMENT_STORE_H

#include <stdbool.h>

#include "management/access_schedule.h"
#include "management/admin_auth.h"

/*
 * NVS persistence for the access schedule and the administrator credentials.
 * The ESP32 adapter calls these on boot and after each protected change.
 */

bool management_store_load(access_schedule_t *schedule, admin_credentials_t *credentials);
bool management_store_save(const access_schedule_t *schedule, const admin_credentials_t *credentials);

#endif /* MANAGEMENT_STORE_H */
