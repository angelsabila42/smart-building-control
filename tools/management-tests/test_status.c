#include <string.h>

#include "test_util.h"

void run_status_tests(void)
{
    system_status_t status;
    char text[2048];

    system_status_init(&status, "test-fw");
    CHECK_STR(status.firmware, "test-fw");
    CHECK_STR(status.rooms[0].name, "Room 1");
    CHECK_STR(status.rooms[1].name, "Room 2");
    CHECK(!status.rooms[0].online);

    status.uptime_seconds = 65;
    status.free_heap = 123456;
    status.min_free_heap = 100000;
    status.wifi_connected = true;
    status.mqtt_connected = false;
    status.sd_mounted = true;
    status.schedule_enabled = true;

    const int64_t now = time_tm_to_epoch(&(struct tm) {
        .tm_year = 126, .tm_mon = 9, .tm_mday = 30,
        .tm_hour = 14, .tm_min = 5, .tm_sec = 0
    });

    status.clock_valid = true;

    system_status_format(&status, now, text, sizeof(text));
    CHECK_CONTAINS(text, "Firmware: test-fw");
    CHECK_CONTAINS(text, "Uptime: 0d 00:01");
    CHECK_CONTAINS(text, "Clock: 2026-10-30 14:05:00 (valid)");
    CHECK_CONTAINS(text, "Heap: 123456 bytes free (minimum 100000)");
    CHECK_CONTAINS(text, "Wi-Fi: connected");
    CHECK_CONTAINS(text, "MQTT: disconnected");
    CHECK_CONTAINS(text, "SD card: mounted");
    CHECK_CONTAINS(text, "Schedule: enabled");
    CHECK_CONTAINS(text, "Override: inactive");
    CHECK_CONTAINS(text, "Building occupancy: unknown");
    CHECK_CONTAINS(text, "Room 1: offline");

    system_status_room_update(&status, 0, true, true, now - 10);
    system_status_room_update(&status, 1, false, false, now);

    CHECK(status.rooms[0].online);
    CHECK(status.rooms[0].occupied);
    CHECK(status.rooms[0].light_on);

    system_status_format(&status, now, text, sizeof(text));
    CHECK_CONTAINS(text, "Room 1: occupied, light ON, updated 10s ago");
    CHECK_CONTAINS(text, "Room 2: unoccupied, light OFF, updated 0s ago");

    /* Unknown values are shown explicitly. */
    status.clock_valid = false;
    status.free_heap = 0;
    status.min_free_heap = 0;
    status.sd_mounted = false;
    status.occupancy_known = true;
    status.building_occupancy = 3;
    status.last_access_event = now;

    system_status_format(&status, now, text, sizeof(text));
    CHECK_CONTAINS(text, "Clock: not set (NOT SET - schedule fails closed)");
    CHECK_CONTAINS(text, "SD card: not mounted");
    CHECK_CONTAINS(text, "Building occupancy: 3");
    CHECK_CONTAINS(text, "Last access event: 2026-10-30 14:05:00");

    /* Rooms go offline after the timeout. */
    system_status_expire_rooms(&status, now + 60, 120);
    CHECK(status.rooms[0].online);
    system_status_expire_rooms(&status, now + 121, 120);
    CHECK(!status.rooms[0].online);
    CHECK(!status.rooms[1].online);

    system_status_format(&status, now, text, sizeof(text));
    CHECK_CONTAINS(text, "Room 1: offline");
    CHECK_CONTAINS(text, "Room 2: offline");
}
