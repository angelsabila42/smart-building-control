#ifndef MANAGEMENT_ACCESS_SCHEDULE_H
#define MANAGEMENT_ACCESS_SCHEDULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define ACCESS_SCHEDULE_DAYS 7
#define ACCESS_SCHEDULE_MAX_WINDOWS 2
#define ACCESS_SCHEDULE_USER_MAX 32
#define ACCESS_SCHEDULE_OVERRIDE_DEFAULT_MINUTES 30

/*
 * Access schedule model.
 *
 * A week is described by one entry per day (0 = Sunday .. 6 = Saturday).
 * Each enabled day has one or two non-overlapping windows, expressed in
 * minutes after midnight.  Window ends are exclusive, so 07:00-21:00 allows
 * access at 07:00 and denies it at 21:00.  An end of 24:00 is allowed.
 *
 * The schedule is evaluated against the terminal's wall clock.  When the
 * global switch is off, access is allowed at any time (for example during
 * commissioning).  A temporary administrator override can allow access while
 * the schedule would otherwise deny it.
 */

typedef struct {
    uint16_t start; /* minutes after midnight, 0 .. 1439 */
    uint16_t end;   /* minutes after midnight, 1 .. 1440 */
} access_window_t;

typedef struct {
    bool enabled;
    uint8_t window_count;
    access_window_t windows[ACCESS_SCHEDULE_MAX_WINDOWS];
} access_schedule_day_t;

typedef struct {
    bool schedule_enabled;
    access_schedule_day_t days[ACCESS_SCHEDULE_DAYS];
    int64_t override_until; /* 0 = no override, otherwise epoch seconds */
    int64_t updated_at;     /* epoch seconds of the last change */
    char updated_by[ACCESS_SCHEDULE_USER_MAX];
} access_schedule_t;

typedef enum {
    ACCESS_ALLOWED = 0,
    ACCESS_ALLOWED_OVERRIDE,
    ACCESS_ALLOWED_SCHEDULE_DISABLED,
    ACCESS_DENIED_DAY_CLOSED,
    ACCESS_DENIED_OUTSIDE_HOURS,
    ACCESS_DENIED_CLOCK_INVALID
} access_decision_t;

typedef enum {
    ACCESS_SCHEDULE_OK = 0,
    ACCESS_SCHEDULE_ERR_DAY,
    ACCESS_SCHEDULE_ERR_TIME_RANGE,
    ACCESS_SCHEDULE_ERR_OVERLAP,
    ACCESS_SCHEDULE_ERR_TOO_MANY_WINDOWS,
    ACCESS_SCHEDULE_ERR_NO_WINDOWS,
    ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS
} access_schedule_error_t;

/* Mon-Fri 07:00-21:00, Sat 08:00-13:00, Sunday closed. */
void access_schedule_defaults(access_schedule_t *schedule);

/* Returns true when the decision allows entry. */
bool access_decision_is_allowed(access_decision_t decision);

/* Human readable reason, suitable for logs and the serial menu. */
const char *access_decision_reason(access_decision_t decision);

/* Validates the whole schedule.  On failure, err receives a short message. */
access_schedule_error_t access_schedule_validate(const access_schedule_t *schedule,
                                                 char *err,
                                                 size_t err_size);

/*
 * Replaces one day's windows.  The schedule is only modified when the new
 * value is valid, so a rejected command never leaves partial configuration.
 */
access_schedule_error_t access_schedule_set_day(access_schedule_t *schedule,
                                                int day,
                                                const access_window_t *windows,
                                                size_t window_count,
                                                char *err,
                                                size_t err_size);

/* Removes all windows and disables the day. */
access_schedule_error_t access_schedule_clear_day(access_schedule_t *schedule, int day);

/* Enables or disables a day that already has windows configured. */
access_schedule_error_t access_schedule_set_day_enabled(access_schedule_t *schedule,
                                                        int day,
                                                        bool enabled);

/*
 * Core access decision for an entry attempt.
 *
 * `when` must describe the terminal's current wall clock.  Pass NULL when the
 * clock has not been set; the decision then fails closed.
 */
access_decision_t access_schedule_check(const access_schedule_t *schedule,
                                        const struct tm *when,
                                        int64_t now_epoch);

/*
 * Exit decisions are never restricted by the schedule: life safety takes
 * precedence over access policy.  This helper exists so callers do not
 * accidentally apply the entry policy to egress.
 */
access_decision_t access_schedule_check_exit(void);

bool access_schedule_override_active(const access_schedule_t *schedule, int64_t now_epoch);

void access_schedule_set_override(access_schedule_t *schedule, int64_t until_epoch);

/* Marks who changed the schedule; used for auditing. */
void access_schedule_touch(access_schedule_t *schedule, int64_t now_epoch, const char *user);

/* Formats the full schedule for the serial interface (no trailing newline). */
void access_schedule_format(const access_schedule_t *schedule,
                            int64_t now_epoch,
                            char *buffer,
                            size_t size);

/* Formats one day, for example "Mon 07:00-21:00". */
void access_schedule_format_day(const access_schedule_t *schedule,
                                int day,
                                char *buffer,
                                size_t size);

#endif /* MANAGEMENT_ACCESS_SCHEDULE_H */
