#include "management/access_schedule.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "management/time_utils.h"

static void set_error(char *err, size_t err_size, const char *message)
{
    if (err != NULL && err_size > 0) {
        snprintf(err, err_size, "%s", message);
    }
}

static bool valid_day(int day)
{
    return day >= 0 && day < ACCESS_SCHEDULE_DAYS;
}

static bool day_windows_valid(const access_schedule_day_t *day, char *err, size_t err_size)
{
    if (day->window_count > ACCESS_SCHEDULE_MAX_WINDOWS) {
        set_error(err, err_size, "too many windows for one day");
        return false;
    }

    if (day->enabled && day->window_count == 0) {
        set_error(err, err_size, "enabled day needs at least one window");
        return false;
    }

    for (size_t i = 0; i < day->window_count; i++) {
        const access_window_t *window = &day->windows[i];

        if (window->start >= window->end) {
            set_error(err, err_size, "window start must be before its end");
            return false;
        }

        if (window->end > TIME_MINUTES_PER_DAY) {
            set_error(err, err_size, "window end is after 24:00");
            return false;
        }
    }

    for (size_t i = 0; i < day->window_count; i++) {
        for (size_t j = i + 1; j < day->window_count; j++) {
            const access_window_t *left = &day->windows[i];
            const access_window_t *right = &day->windows[j];

            if (left->start < right->end && right->start < left->end) {
                set_error(err, err_size, "windows overlap");
                return false;
            }
        }
    }

    return true;
}

void access_schedule_defaults(access_schedule_t *schedule)
{
    if (schedule == NULL) {
        return;
    }

    memset(schedule, 0, sizeof(*schedule));

    schedule->schedule_enabled = true;

    for (int day = 0; day < ACCESS_SCHEDULE_DAYS; day++) {
        access_schedule_day_t *entry = &schedule->days[day];

        if (day >= 1 && day <= 5) {
            entry->enabled = true;
            entry->window_count = 1;
            entry->windows[0].start = 7 * 60;
            entry->windows[0].end = 21 * 60;
        } else if (day == 6) {
            entry->enabled = true;
            entry->window_count = 1;
            entry->windows[0].start = 8 * 60;
            entry->windows[0].end = 13 * 60;
        }
    }

    snprintf(schedule->updated_by, sizeof(schedule->updated_by), "%s", "default");
}

bool access_decision_is_allowed(access_decision_t decision)
{
    return decision == ACCESS_ALLOWED ||
           decision == ACCESS_ALLOWED_OVERRIDE ||
           decision == ACCESS_ALLOWED_SCHEDULE_DISABLED;
}

const char *access_decision_reason(access_decision_t decision)
{
    switch (decision) {
        case ACCESS_ALLOWED:
            return "inside schedule window";
        case ACCESS_ALLOWED_OVERRIDE:
            return "administrator override active";
        case ACCESS_ALLOWED_SCHEDULE_DISABLED:
            return "schedule disabled";
        case ACCESS_DENIED_DAY_CLOSED:
            return "no access configured for this day";
        case ACCESS_DENIED_OUTSIDE_HOURS:
            return "outside scheduled hours";
        case ACCESS_DENIED_CLOCK_INVALID:
            return "clock not set; failing closed";
        default:
            return "unknown decision";
    }
}

access_schedule_error_t access_schedule_validate(const access_schedule_t *schedule,
                                                 char *err,
                                                 size_t err_size)
{
    if (schedule == NULL) {
        set_error(err, err_size, "schedule is missing");
        return ACCESS_SCHEDULE_ERR_DAY;
    }

    bool has_active_day = false;

    for (int day = 0; day < ACCESS_SCHEDULE_DAYS; day++) {
        if (!day_windows_valid(&schedule->days[day], err, err_size)) {
            return ACCESS_SCHEDULE_ERR_TIME_RANGE;
        }

        if (schedule->days[day].enabled) {
            has_active_day = true;
        }
    }

    if (schedule->schedule_enabled && !has_active_day) {
        set_error(err, err_size, "enable at least one day or switch the schedule off");
        return ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS;
    }

    return ACCESS_SCHEDULE_OK;
}

access_schedule_error_t access_schedule_set_day(access_schedule_t *schedule,
                                                int day,
                                                const access_window_t *windows,
                                                size_t window_count,
                                                char *err,
                                                size_t err_size)
{
    if (schedule == NULL || !valid_day(day)) {
        set_error(err, err_size, "invalid day (use sun..sat)");
        return ACCESS_SCHEDULE_ERR_DAY;
    }

    if (window_count == 0) {
        set_error(err, err_size, "at least one window is required");
        return ACCESS_SCHEDULE_ERR_NO_WINDOWS;
    }

    if (window_count > ACCESS_SCHEDULE_MAX_WINDOWS) {
        set_error(err, err_size, "at most two windows per day");
        return ACCESS_SCHEDULE_ERR_TOO_MANY_WINDOWS;
    }

    if (windows == NULL) {
        set_error(err, err_size, "windows are missing");
        return ACCESS_SCHEDULE_ERR_NO_WINDOWS;
    }

    access_schedule_day_t candidate = { 0 };

    candidate.enabled = true;
    candidate.window_count = (uint8_t)window_count;
    memcpy(candidate.windows, windows, window_count * sizeof(access_window_t));

    if (!day_windows_valid(&candidate, err, err_size)) {
        return ACCESS_SCHEDULE_ERR_TIME_RANGE;
    }

    /* Validate the complete schedule before touching the live copy. */
    access_schedule_t backup = *schedule;

    schedule->days[day] = candidate;

    if (schedule->schedule_enabled) {
        bool has_active_day = false;

        for (int i = 0; i < ACCESS_SCHEDULE_DAYS; i++) {
            if (schedule->days[i].enabled) {
                has_active_day = true;
            }
        }

        if (!has_active_day) {
            *schedule = backup;
            set_error(err, err_size, "enable at least one day or switch the schedule off");
            return ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS;
        }
    }

    return ACCESS_SCHEDULE_OK;
}

access_schedule_error_t access_schedule_clear_day(access_schedule_t *schedule, int day)
{
    if (schedule == NULL || !valid_day(day)) {
        return ACCESS_SCHEDULE_ERR_DAY;
    }

    access_schedule_t candidate = *schedule;
    candidate.days[day].enabled = false;
    candidate.days[day].window_count = 0;
    memset(candidate.days[day].windows, 0, sizeof(candidate.days[day].windows));

    if (candidate.schedule_enabled) {
        bool has_active_day = false;

        for (int i = 0; i < ACCESS_SCHEDULE_DAYS; i++) {
            if (candidate.days[i].enabled) {
                has_active_day = true;
            }
        }

        if (!has_active_day) {
            return ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS;
        }
    }

    *schedule = candidate;
    return ACCESS_SCHEDULE_OK;
}

access_schedule_error_t access_schedule_set_day_enabled(access_schedule_t *schedule,
                                                        int day,
                                                        bool enabled)
{
    if (schedule == NULL || !valid_day(day)) {
        return ACCESS_SCHEDULE_ERR_DAY;
    }

    if (enabled) {
        if (schedule->days[day].window_count == 0) {
            return ACCESS_SCHEDULE_ERR_NO_WINDOWS;
        }

        schedule->days[day].enabled = true;
        return ACCESS_SCHEDULE_OK;
    }

    /* Disabling keeps the configured windows so 'enable' can restore them. */
    if (schedule->schedule_enabled) {
        bool has_other_active_day = false;

        for (int i = 0; i < ACCESS_SCHEDULE_DAYS; i++) {
            if (i != day && schedule->days[i].enabled) {
                has_other_active_day = true;
            }
        }

        if (!has_other_active_day) {
            return ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS;
        }
    }

    schedule->days[day].enabled = false;
    return ACCESS_SCHEDULE_OK;
}

access_decision_t access_schedule_check(const access_schedule_t *schedule,
                                        const struct tm *when,
                                        int64_t now_epoch)
{
    if (schedule == NULL) {
        return ACCESS_DENIED_CLOCK_INVALID;
    }

    if (!schedule->schedule_enabled) {
        return ACCESS_ALLOWED_SCHEDULE_DISABLED;
    }

    if (when == NULL) {
        return ACCESS_DENIED_CLOCK_INVALID;
    }

    if (access_schedule_override_active(schedule, now_epoch)) {
        return ACCESS_ALLOWED_OVERRIDE;
    }

    const int day = time_day_index(when->tm_wday);
    const access_schedule_day_t *entry = &schedule->days[day];

    if (!entry->enabled || entry->window_count == 0) {
        return ACCESS_DENIED_DAY_CLOSED;
    }

    const uint16_t minute_of_day = (uint16_t)(when->tm_hour * 60 + when->tm_min);

    for (size_t i = 0; i < entry->window_count; i++) {
        if (minute_of_day >= entry->windows[i].start &&
            minute_of_day < entry->windows[i].end) {
            return ACCESS_ALLOWED;
        }
    }

    return ACCESS_DENIED_OUTSIDE_HOURS;
}

access_decision_t access_schedule_check_exit(void)
{
    /* Never restrict egress: a schedule must not trap anyone inside. */
    return ACCESS_ALLOWED;
}

bool access_schedule_override_active(const access_schedule_t *schedule, int64_t now_epoch)
{
    return schedule != NULL &&
           schedule->override_until > 0 &&
           schedule->override_until > now_epoch;
}

void access_schedule_set_override(access_schedule_t *schedule, int64_t until_epoch)
{
    if (schedule != NULL) {
        schedule->override_until = until_epoch;
    }
}

void access_schedule_touch(access_schedule_t *schedule, int64_t now_epoch, const char *user)
{
    if (schedule == NULL) {
        return;
    }

    schedule->updated_at = now_epoch;

    if (user != NULL && user[0] != '\0') {
        snprintf(schedule->updated_by, sizeof(schedule->updated_by), "%s", user);
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

void access_schedule_format_day(const access_schedule_t *schedule,
                                int day,
                                char *buffer,
                                size_t size)
{
    if (schedule == NULL || buffer == NULL || size == 0 || !valid_day(day)) {
        return;
    }

    writer_t writer = { buffer, size, 0 };
    const access_schedule_day_t *entry = &schedule->days[day];

    writer_append(&writer, "%s ", time_day_short_name(day));

    if (!entry->enabled || entry->window_count == 0) {
        writer_append(&writer, "closed");
        return;
    }

    for (size_t i = 0; i < entry->window_count; i++) {
        char start[8];
        char end[8];

        time_format_hhmm(entry->windows[i].start, start, sizeof(start));
        time_format_hhmm(entry->windows[i].end, end, sizeof(end));

        writer_append(&writer, "%s%s-%s", i == 0 ? "" : ", ", start, end);
    }
}

void access_schedule_format(const access_schedule_t *schedule,
                            int64_t now_epoch,
                            char *buffer,
                            size_t size)
{
    if (schedule == NULL || buffer == NULL || size == 0) {
        return;
    }

    writer_t writer = { buffer, size, 0 };

    writer_append(&writer,
                  "Access schedule: %s\n",
                  schedule->schedule_enabled ? "ENABLED" : "DISABLED");

    for (int day = 0; day < ACCESS_SCHEDULE_DAYS; day++) {
        char line[96];

        access_schedule_format_day(schedule, day, line, sizeof(line));
        writer_append(&writer, "  %s\n", line);
    }

    if (access_schedule_override_active(schedule, now_epoch)) {
        struct tm until;
        char stamp[24];

        time_epoch_to_tm(schedule->override_until, &until);
        time_format_datetime(&until, stamp, sizeof(stamp));
        writer_append(&writer, "Override: ACTIVE until %s\n", stamp);
    } else {
        writer_append(&writer, "Override: inactive\n");
    }

    if (schedule->updated_at > 0) {
        struct tm updated;
        char stamp[24];

        time_epoch_to_tm(schedule->updated_at, &updated);
        time_format_datetime(&updated, stamp, sizeof(stamp));

        writer_append(&writer,
                      "Last change: %s by %s",
                      stamp,
                      schedule->updated_by[0] != '\0' ? schedule->updated_by : "unknown");
    } else {
        writer_append(&writer, "Last change: never");
    }
}

