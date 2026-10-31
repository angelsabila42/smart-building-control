#include "management/system_status.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "management/time_utils.h"

#define ROOM_OFFLINE_AFTER_SECONDS 120

void system_status_init(system_status_t *status, const char *firmware)
{
    if (status == NULL) {
        return;
    }

    memset(status, 0, sizeof(*status));

    snprintf(status->firmware,
             sizeof(status->firmware),
             "%s",
             firmware != NULL ? firmware : "unknown");

    snprintf(status->rooms[0].name, sizeof(status->rooms[0].name), "%s", "Room 1");
    snprintf(status->rooms[1].name, sizeof(status->rooms[1].name), "%s", "Room 2");
}

void system_status_room_update(system_status_t *status,
                               int room_index,
                               bool occupied,
                               bool light_on,
                               int64_t now_epoch)
{
    if (status == NULL || room_index < 0 || room_index >= SYSTEM_STATUS_ROOMS) {
        return;
    }

    system_status_room_t *room = &status->rooms[room_index];

    room->occupied = occupied;
    room->light_on = light_on;
    room->last_update = now_epoch;
    room->online = true;
}

void system_status_expire_rooms(system_status_t *status, int64_t now_epoch, int64_t timeout_seconds)
{
    if (status == NULL) {
        return;
    }

    if (timeout_seconds <= 0) {
        timeout_seconds = ROOM_OFFLINE_AFTER_SECONDS;
    }

    for (int i = 0; i < SYSTEM_STATUS_ROOMS; i++) {
        system_status_room_t *room = &status->rooms[i];

        if (room->online &&
            (room->last_update <= 0 || now_epoch - room->last_update > timeout_seconds)) {
            room->online = false;
        }
    }
}

typedef struct {
    char *buffer;
    size_t size;
    size_t length;
} writer_t;

static void writer_append(writer_t *writer, const char *format, ...)
{
    if (writer->length >= writer->size) {
        return;
    }

    va_list args;
    va_start(args, format);
    const int written = vsnprintf(writer->buffer + writer->length,
                                  writer->size - writer->length,
                                  format,
                                  args);
    va_end(args);

    if (written < 0) {
        return;
    }

    if ((size_t)written >= writer->size - writer->length) {
        writer->length = writer->size;
    } else {
        writer->length += (size_t)written;
    }
}

static void format_uptime(int64_t seconds, char *buffer, size_t size)
{
    if (seconds < 0) {
        seconds = 0;
    }

    const int64_t days = seconds / 86400;
    const int64_t hours = (seconds % 86400) / 3600;
    const int64_t minutes = (seconds % 3600) / 60;

    snprintf(buffer, size, "%lldd %02lld:%02lld",
             (long long)days,
             (long long)hours,
             (long long)minutes);
}

void system_status_format(const system_status_t *status,
                          int64_t now_epoch,
                          char *buffer,
                          size_t size)
{
    if (status == NULL || buffer == NULL || size == 0) {
        return;
    }

    writer_t writer = { buffer, size, 0 };
    char uptime[24];
    char clock[32] = "not set";

    format_uptime(status->uptime_seconds, uptime, sizeof(uptime));

    if (status->clock_valid && now_epoch > 0) {
        struct tm value;
        char stamp[24];

        time_epoch_to_tm(now_epoch, &value);
        time_format_datetime(&value, stamp, sizeof(stamp));
        snprintf(clock, sizeof(clock), "%s", stamp);
    }

    writer_append(&writer, "Firmware: %s\n", status->firmware);
    writer_append(&writer, "Uptime: %s\n", uptime);

    if (status->clock_valid) {
        writer_append(&writer, "Clock: %s (valid)\n", clock);
    } else {
        writer_append(&writer, "Clock: %s (NOT SET - schedule fails closed)\n", clock);
    }

    writer_append(&writer,
                  "Heap: %lu bytes free (minimum %lu)\n",
                  (unsigned long)status->free_heap,
                  (unsigned long)status->min_free_heap);
    writer_append(&writer,
                  "Wi-Fi: %-12s  MQTT: %s\n",
                  status->wifi_connected ? "connected" : "disconnected",
                  status->mqtt_connected ? "connected" : "disconnected");
    writer_append(&writer,
                  "SD card: %-10s  Storage log: %s\n",
                  status->sd_mounted ? "mounted" : "not mounted",
                  status->sd_mounted ? "available" : "RAM only");
    writer_append(&writer,
                  "Schedule: %-9s  Override: %s\n",
                  status->schedule_enabled ? "enabled" : "disabled",
                  status->override_active ? "ACTIVE" : "inactive");

    if (status->occupancy_known) {
        writer_append(&writer, "Building occupancy: %d\n", status->building_occupancy);
    } else {
        writer_append(&writer, "Building occupancy: unknown (no access events yet)\n");
    }

    if (status->last_access_event > 0) {
        struct tm last;
        char stamp[24];

        time_epoch_to_tm(status->last_access_event, &last);
        time_format_datetime(&last, stamp, sizeof(stamp));
        writer_append(&writer, "Last access event: %s\n", stamp);
    }

    writer_append(&writer, "Rooms:");

    for (int i = 0; i < SYSTEM_STATUS_ROOMS; i++) {
        const system_status_room_t *room = &status->rooms[i];

        if (!room->online) {
            writer_append(&writer, "\n  %s: offline", room->name);
            continue;
        }

        if (room->last_update > 0 && now_epoch >= room->last_update) {
            writer_append(&writer,
                          "\n  %s: %s, light %s, updated %llds ago",
                          room->name,
                          room->occupied ? "occupied" : "unoccupied",
                          room->light_on ? "ON" : "OFF",
                          (long long)(now_epoch - room->last_update));
        } else {
            writer_append(&writer,
                          "\n  %s: %s, light %s, update time unknown",
                          room->name,
                          room->occupied ? "occupied" : "unoccupied",
                          room->light_on ? "ON" : "OFF");
        }
    }
}
