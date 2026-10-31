#ifndef MANAGEMENT_TIME_UTILS_H
#define MANAGEMENT_TIME_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define TIME_MINUTES_PER_DAY 1440
#define TIME_SECONDS_PER_DAY 86400
#define TIME_DAYS_PER_WEEK 7

/*
 * Time helpers used by the management interface.
 *
 * The device keeps a wall-clock time (seconds since the Unix epoch).  Schedule
 * decisions always compare the wall-clock fields of a struct tm, so the same
 * functions work on the ESP32 and on a development host.
 */

/* Parses "HH:MM" (00:00 - 23:59). */
bool time_parse_hhmm(const char *text, uint16_t *minutes_out);

/* Parses "HH:MM" and additionally accepts "24:00" as the end of a day window. */
bool time_parse_hhmm_end(const char *text, uint16_t *minutes_out);

/* Formats minutes as "HH:MM"; 1440 becomes "24:00". */
void time_format_hhmm(uint16_t minutes, char *buffer, size_t size);

/* Parses "YYYY-MM-DD HH:MM:SS" (24-hour clock). */
bool time_parse_datetime(const char *text, struct tm *out);

/* Formats a struct tm as "YYYY-MM-DD HH:MM:SS". */
void time_format_datetime(const struct tm *value, char *buffer, size_t size);

/* Normalises a tm_wday value into 0 (Sunday) .. 6 (Saturday). */
int time_day_index(int tm_wday);

/* Day names: 0 = Sunday. */
const char *time_day_short_name(int day);
const char *time_day_long_name(int day);

/* Parses "sun"/"sunday" style names (case insensitive). */
bool time_parse_day(const char *text, int *day_index);

/* Civil-calendar conversions.  Both use UTC-style arithmetic and ignore the
 * tm_wday/tm_yday fields of the input. */
int64_t time_tm_to_epoch(const struct tm *value);
void time_epoch_to_tm(int64_t epoch_seconds, struct tm *out);

/* True when the value looks like a real, already-synchronised clock. */
bool time_epoch_is_plausible(int64_t epoch_seconds);

#endif /* MANAGEMENT_TIME_UTILS_H */
