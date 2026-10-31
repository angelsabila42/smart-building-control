#ifndef MANAGEMENT_SYSTEM_STATUS_H
#define MANAGEMENT_SYSTEM_STATUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYSTEM_STATUS_ROOMS 2
#define SYSTEM_STATUS_NAME_MAX 16
#define SYSTEM_STATUS_FIRMWARE_MAX 16

/*
 * Snapshot of everything the management interface reports.  The ESP32 adapter
 * fills in the hardware fields; room status is expected to be updated from the
 * MQTT messages published by the two lecture-room terminals.
 */

typedef struct {
    char name[SYSTEM_STATUS_NAME_MAX];
    bool online;    /* a room status message has arrived recently */
    bool occupied;
    bool light_on;
    int64_t last_update; /* epoch seconds */
} system_status_room_t;

typedef struct {
    char firmware[SYSTEM_STATUS_FIRMWARE_MAX];
    int64_t uptime_seconds;
    bool clock_valid;
    uint32_t free_heap;
    uint32_t min_free_heap;
    bool wifi_connected;
    bool mqtt_connected;
    bool sd_mounted;
    bool schedule_enabled;
    bool override_active;
    int building_occupancy;
    bool occupancy_known;
    int64_t last_access_event;
    system_status_room_t rooms[SYSTEM_STATUS_ROOMS];
} system_status_t;

void system_status_init(system_status_t *status, const char *firmware);

void system_status_room_update(system_status_t *status,
                               int room_index,
                               bool occupied,
                               bool light_on,
                               int64_t now_epoch);

/* Marks rooms offline when no update was received within the timeout. */
void system_status_expire_rooms(system_status_t *status, int64_t now_epoch, int64_t timeout_seconds);

/* Formats the status report (no trailing newline). */
void system_status_format(const system_status_t *status,
                          int64_t now_epoch,
                          char *buffer,
                          size_t size);

#endif /* MANAGEMENT_SYSTEM_STATUS_H */
