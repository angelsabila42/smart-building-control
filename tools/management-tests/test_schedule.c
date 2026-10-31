#include <string.h>

#include "test_util.h"

static struct tm monday_at(int hour, int minute)
{
    /* 2026-10-26 is a Monday. */
    return tm_at(2026, 10, 26, hour, minute);
}

static struct tm sunday_at(int hour, int minute)
{
    /* 2026-11-01 is a Sunday. */
    return tm_at(2026, 11, 1, hour, minute);
}

void run_schedule_tests(void)
{
    access_schedule_t schedule;
    char error[96];

    access_schedule_defaults(&schedule);
    CHECK(access_schedule_validate(&schedule, error, sizeof(error)) == ACCESS_SCHEDULE_OK);
    CHECK(schedule.schedule_enabled);
    CHECK(schedule.days[1].enabled);
    CHECK_INT(schedule.days[1].windows[0].start, 7 * 60);
    CHECK_INT(schedule.days[1].windows[0].end, 21 * 60);
    CHECK(schedule.days[6].enabled);
    CHECK(!schedule.days[0].enabled);

    struct tm monday = monday_at(10, 0);

    CHECK_INT(access_schedule_check(&schedule, &monday, 0), ACCESS_ALLOWED);

    struct tm edge = monday_at(7, 0);
    CHECK_INT(access_schedule_check(&schedule, &edge, 0), ACCESS_ALLOWED);

    edge = monday_at(20, 59);
    CHECK_INT(access_schedule_check(&schedule, &edge, 0), ACCESS_ALLOWED);

    edge = monday_at(21, 0);
    CHECK_INT(access_schedule_check(&schedule, &edge, 0), ACCESS_DENIED_OUTSIDE_HOURS);

    edge = monday_at(6, 59);
    CHECK_INT(access_schedule_check(&schedule, &edge, 0), ACCESS_DENIED_OUTSIDE_HOURS);

    struct tm sunday = sunday_at(12, 0);
    CHECK_INT(access_schedule_check(&schedule, &sunday, 0), ACCESS_DENIED_DAY_CLOSED);

    CHECK_INT(access_schedule_check(&schedule, NULL, 0), ACCESS_DENIED_CLOCK_INVALID);

    CHECK(access_decision_is_allowed(ACCESS_ALLOWED));
    CHECK(access_decision_is_allowed(ACCESS_ALLOWED_OVERRIDE));
    CHECK(access_decision_is_allowed(ACCESS_ALLOWED_SCHEDULE_DISABLED));
    CHECK(!access_decision_is_allowed(ACCESS_DENIED_OUTSIDE_HOURS));
    CHECK(!access_decision_is_allowed(ACCESS_DENIED_DAY_CLOSED));
    CHECK(!access_decision_is_allowed(ACCESS_DENIED_CLOCK_INVALID));
    CHECK_STR(access_decision_reason(ACCESS_DENIED_OUTSIDE_HOURS), "outside scheduled hours");

    /* Disabling enforcement allows any time. */
    schedule.schedule_enabled = false;
    CHECK_INT(access_schedule_check(&schedule, &sunday, 0), ACCESS_ALLOWED_SCHEDULE_DISABLED);
    CHECK_INT(access_schedule_check(&schedule, &monday, 0), ACCESS_ALLOWED_SCHEDULE_DISABLED);
    schedule.schedule_enabled = true;

    /* Administrator override, with expiry. */
    access_schedule_set_override(&schedule, 1000);
    CHECK(access_schedule_override_active(&schedule, 999));
    CHECK(!access_schedule_override_active(&schedule, 1000));
    CHECK_INT(access_schedule_check(&schedule, &sunday, 999), ACCESS_ALLOWED_OVERRIDE);
    CHECK_INT(access_schedule_check(&schedule, &sunday, 1000), ACCESS_DENIED_DAY_CLOSED);
    access_schedule_set_override(&schedule, 0);

    /* Setting two windows. */
    const access_window_t windows[] = {
        { .start = 8 * 60, .end = 12 * 60 },
        { .start = 14 * 60, .end = 18 * 60 },
    };

    access_schedule_defaults(&schedule);
    CHECK(access_schedule_set_day(&schedule, 1, windows, 2, error, sizeof(error)) ==
          ACCESS_SCHEDULE_OK);
    CHECK_INT(schedule.days[1].window_count, 2);

    edge = monday_at(13, 0);
    CHECK_INT(access_schedule_check(&schedule, &edge, 0), ACCESS_DENIED_OUTSIDE_HOURS);
    edge = monday_at(15, 0);
    CHECK_INT(access_schedule_check(&schedule, &edge, 0), ACCESS_ALLOWED);

    char text[512];

    access_schedule_format_day(&schedule, 1, text, sizeof(text));
    CHECK_STR(text, "Mon 08:00-12:00, 14:00-18:00");
    access_schedule_format_day(&schedule, 0, text, sizeof(text));
    CHECK_STR(text, "Sun closed");

    access_schedule_format(&schedule, 0, text, sizeof(text));
    CHECK_CONTAINS(text, "Access schedule: ENABLED");
    CHECK_CONTAINS(text, "Mon 08:00-12:00, 14:00-18:00");
    CHECK_CONTAINS(text, "Override: inactive");

    /* Overlapping or inverted windows are rejected and leave the day intact. */
    const access_window_t overlap[] = {
        { .start = 7 * 60, .end = 13 * 60 },
        { .start = 12 * 60, .end = 15 * 60 },
    };
    const uint8_t windows_before = schedule.days[1].window_count;

    CHECK(access_schedule_set_day(&schedule, 1, overlap, 2, error, sizeof(error)) ==
          ACCESS_SCHEDULE_ERR_TIME_RANGE);
    CHECK_INT(schedule.days[1].window_count, windows_before);
    CHECK_STR(error, "windows overlap");

    const access_window_t inverted[] = { { .start = 15 * 60, .end = 15 * 60 } };

    CHECK(access_schedule_set_day(&schedule, 1, inverted, 1, error, sizeof(error)) ==
          ACCESS_SCHEDULE_ERR_TIME_RANGE);

    const access_window_t too_many[] = {
        { .start = 1, .end = 2 }, { .start = 3, .end = 4 }, { .start = 5, .end = 6 }
    };

    CHECK(access_schedule_set_day(&schedule, 1, too_many, 3, error, sizeof(error)) ==
          ACCESS_SCHEDULE_ERR_TOO_MANY_WINDOWS);

    /* 24:00 is a valid end. */
    const access_window_t late[] = { { .start = 18 * 60, .end = 1440 } };

    CHECK(access_schedule_set_day(&schedule, 1, late, 1, error, sizeof(error)) ==
          ACCESS_SCHEDULE_OK);
    edge = monday_at(23, 59);
    CHECK_INT(access_schedule_check(&schedule, &edge, 0), ACCESS_ALLOWED);

    /* Closing every day is rejected; 'schedule off' is the supported path. */
    access_schedule_defaults(&schedule);

    for (int day = 0; day < ACCESS_SCHEDULE_DAYS; day++) {
        if (day == 1) {
            continue;
        }

        CHECK(access_schedule_clear_day(&schedule, day) == ACCESS_SCHEDULE_OK);
    }

    CHECK(access_schedule_clear_day(&schedule, 1) == ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS);
    CHECK(schedule.days[1].enabled);

    /* Disabling keeps the configured windows so 'enable' can restore them. */
    CHECK(access_schedule_set_day_enabled(&schedule, 1, false) ==
          ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS);

    access_schedule_defaults(&schedule);
    CHECK(access_schedule_set_day_enabled(&schedule, 1, false) == ACCESS_SCHEDULE_OK);
    CHECK(!schedule.days[1].enabled);
    CHECK_INT(schedule.days[1].window_count, 1);
    CHECK(access_schedule_set_day_enabled(&schedule, 1, true) == ACCESS_SCHEDULE_OK);
    CHECK(schedule.days[1].enabled);

    /* Invalid day indexes. */
    CHECK(access_schedule_set_day(&schedule, 9, windows, 1, error, sizeof(error)) ==
          ACCESS_SCHEDULE_ERR_DAY);
    CHECK(access_schedule_clear_day(&schedule, -1) == ACCESS_SCHEDULE_ERR_DAY);

    /* Exit is never restricted by the schedule. */
    CHECK_INT(access_schedule_check_exit(), ACCESS_ALLOWED);
}
