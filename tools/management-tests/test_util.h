#ifndef MANAGEMENT_TEST_UTIL_H
#define MANAGEMENT_TEST_UTIL_H

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "management/access_events.h"
#include "management/access_schedule.h"
#include "management/admin_auth.h"
#include "management/serial_menu.h"
#include "management/system_status.h"
#include "management/time_utils.h"

extern int tests_run;
extern int tests_failed;

#define CHECK(condition)                                                           \
    do {                                                                           \
        tests_run++;                                                               \
        if (!(condition)) {                                                        \
            tests_failed++;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);            \
        }                                                                          \
    } while (0)

#define CHECK_INT(actual, expected)                                                \
    do {                                                                           \
        const long long actual_value = (long long)(actual);                        \
        const long long expected_value = (long long)(expected);                    \
        tests_run++;                                                               \
        if (actual_value != expected_value) {                                      \
            tests_failed++;                                                        \
            printf("FAIL %s:%d: %s == %lld, expected %lld\n",                      \
                   __FILE__, __LINE__, #actual, actual_value, expected_value);     \
        }                                                                          \
    } while (0)

#define CHECK_STR(actual, expected)                                                \
    do {                                                                           \
        const char *actual_value = (actual);                                       \
        const char *expected_value = (expected);                                   \
        tests_run++;                                                               \
        if (actual_value == NULL || strcmp(actual_value, expected_value) != 0) {   \
            tests_failed++;                                                        \
            printf("FAIL %s:%d: %s == \"%s\", expected \"%s\"\n",                  \
                   __FILE__, __LINE__, #actual,                                    \
                   actual_value != NULL ? actual_value : "(null)",                 \
                   expected_value);                                                \
        }                                                                          \
    } while (0)

#define CHECK_CONTAINS(haystack, needle)                                           \
    do {                                                                           \
        const char *haystack_value = (haystack);                                   \
        const char *needle_value = (needle);                                       \
        tests_run++;                                                               \
        if (haystack_value == NULL || strstr(haystack_value, needle_value) == NULL) { \
            tests_failed++;                                                        \
            printf("FAIL %s:%d: output does not contain \"%s\"\nOutput:\n%s\n",    \
                   __FILE__, __LINE__, needle_value,                               \
                   haystack_value != NULL ? haystack_value : "(null)");            \
        }                                                                          \
    } while (0)

/* Builds a struct tm with the correct weekday for the given civil date. */
static inline struct tm tm_at(int year, int month, int day, int hour, int minute)
{
    struct tm value;

    memset(&value, 0, sizeof(value));
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = 0;
    value.tm_wday = time_day_index((int)((time_tm_to_epoch(&value) / 86400 + 4) % 7));

    return value;
}

void run_time_tests(void);
void run_schedule_tests(void);
void run_auth_tests(void);
void run_events_tests(void);
void run_status_tests(void);
void run_menu_tests(void);

#endif /* MANAGEMENT_TEST_UTIL_H */
