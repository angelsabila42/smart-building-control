#include "management/time_utils.h"

#include <stdio.h>
#include <string.h>

/* 2020-01-01 00:00:00 UTC.  Anything older is treated as "clock not set". */
#define TIME_PLAUSIBLE_EPOCH 1577836800LL

static bool is_digit(char value)
{
    return value >= '0' && value <= '9';
}

static int digits_to_int(const char *text, size_t count)
{
    int value = 0;

    for (size_t i = 0; i < count; i++) {
        value = value * 10 + (text[i] - '0');
    }

    return value;
}

static bool parse_hhmm_internal(const char *text, uint16_t *minutes_out, bool allow_24_00)
{
    if (text == NULL || minutes_out == NULL) {
        return false;
    }

    if (strlen(text) != 5 || text[2] != ':') {
        return false;
    }

    if (!is_digit(text[0]) || !is_digit(text[1]) ||
        !is_digit(text[3]) || !is_digit(text[4])) {
        return false;
    }

    int hours = digits_to_int(text, 2);
    int minutes = digits_to_int(text + 3, 2);

    if (minutes > 59) {
        return false;
    }

    if (allow_24_00 && hours == 24) {
        if (minutes != 0) {
            return false;
        }

        *minutes_out = TIME_MINUTES_PER_DAY;
        return true;
    }

    if (hours > 23) {
        return false;
    }

    *minutes_out = (uint16_t)(hours * 60 + minutes);
    return true;
}

bool time_parse_hhmm(const char *text, uint16_t *minutes_out)
{
    return parse_hhmm_internal(text, minutes_out, false);
}

bool time_parse_hhmm_end(const char *text, uint16_t *minutes_out)
{
    return parse_hhmm_internal(text, minutes_out, true);
}

void time_format_hhmm(uint16_t minutes, char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        return;
    }

    if (minutes > TIME_MINUTES_PER_DAY) {
        minutes = TIME_MINUTES_PER_DAY;
    }

    snprintf(buffer, size, "%02u:%02u", (unsigned)(minutes / 60), (unsigned)(minutes % 60));
}

/* Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm). */
static int64_t days_from_civil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = (unsigned)(year - era * 400);
    const unsigned shifted_month = (unsigned)((int)month + (month > 2 ? -3 : 9));
    const unsigned day_of_year = (153u * shifted_month + 2u) / 5u + day - 1u;
    const unsigned day_of_era =
        year_of_era * 365u + year_of_era / 4u - year_of_era / 100u + day_of_year;

    return era * 146097 + (int64_t)day_of_era - 719468;
}

static void civil_from_days(int64_t days, int *year, unsigned *month, unsigned *day)
{
    days += 719468;

    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned day_of_era = (unsigned)(days - era * 146097);
    const unsigned year_of_era =
        (day_of_era - day_of_era / 1460u + day_of_era / 36524u - day_of_era / 146096u) / 365u;
    const int computed_year = (int)year_of_era + (int)(era * 400);
    const unsigned day_of_year =
        day_of_era - (365u * year_of_era + year_of_era / 4u - year_of_era / 100u);
    const unsigned month_prime = (5u * day_of_year + 2u) / 153u;
    const unsigned computed_day = day_of_year - (153u * month_prime + 2u) / 5u + 1u;
    const unsigned computed_month = (unsigned)((int)month_prime + (month_prime < 10u ? 3 : -9));

    *year = computed_year + (computed_month <= 2u);
    *month = computed_month;
    *day = computed_day;
}

static bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 2 && is_leap_year(year)) {
        return 29;
    }

    return days[month - 1];
}

bool time_parse_datetime(const char *text, struct tm *out)
{
    if (text == NULL || out == NULL) {
        return false;
    }

    /* Expected shape: YYYY-MM-DD HH:MM:SS */
    if (strlen(text) != 19) {
        return false;
    }

    if (text[4] != '-' || text[7] != '-' || text[10] != ' ' ||
        text[13] != ':' || text[16] != ':') {
        return false;
    }

    static const size_t digit_positions[] = { 0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18 };

    for (size_t i = 0; i < sizeof(digit_positions) / sizeof(digit_positions[0]); i++) {
        if (!is_digit(text[digit_positions[i]])) {
            return false;
        }
    }

    int year = digits_to_int(text, 4);
    int month = digits_to_int(text + 5, 2);
    int day = digits_to_int(text + 8, 2);
    int hour = digits_to_int(text + 11, 2);
    int minute = digits_to_int(text + 14, 2);
    int second = digits_to_int(text + 17, 2);

    if (year < 2000 || year > 2100) {
        return false;
    }

    if (month < 1 || month > 12) {
        return false;
    }

    if (day < 1 || day > days_in_month(year, month)) {
        return false;
    }

    if (hour > 23 || minute > 59 || second > 59) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->tm_year = year - 1900;
    out->tm_mon = month - 1;
    out->tm_mday = day;
    out->tm_hour = hour;
    out->tm_min = minute;
    out->tm_sec = second;
    out->tm_wday = (int)((days_from_civil(year, (unsigned)month, (unsigned)day) + 4) % 7);
    if (out->tm_wday < 0) {
        out->tm_wday += 7;
    }
    out->tm_isdst = 0;

    return true;
}

void time_format_datetime(const struct tm *value, char *buffer, size_t size)
{
    if (value == NULL || buffer == NULL || size == 0) {
        return;
    }

    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d",
             value->tm_year + 1900,
             value->tm_mon + 1,
             value->tm_mday,
             value->tm_hour,
             value->tm_min,
             value->tm_sec);
}

bool time_epoch_is_plausible(int64_t epoch_seconds)
{
    return epoch_seconds >= TIME_PLAUSIBLE_EPOCH;
}

int time_day_index(int tm_wday)
{
    int day = tm_wday % 7;

    if (day < 0) {
        day += 7;
    }

    return day;
}

const char *time_day_short_name(int day)
{
    static const char *names[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    return names[time_day_index(day)];
}

const char *time_day_long_name(int day)
{
    static const char *names[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };

    return names[time_day_index(day)];
}

static bool equals_ignore_case(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        char a = *left;
        char b = *right;

        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }

        if (a != b) {
            return false;
        }

        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

bool time_parse_day(const char *text, int *day_index)
{
    if (text == NULL || day_index == NULL) {
        return false;
    }

    for (int day = 0; day < TIME_DAYS_PER_WEEK; day++) {
        if (equals_ignore_case(text, time_day_short_name(day)) ||
            equals_ignore_case(text, time_day_long_name(day))) {
            *day_index = day;
            return true;
        }
    }

    return false;
}

int64_t time_tm_to_epoch(const struct tm *value)
{
    if (value == NULL) {
        return 0;
    }

    const int64_t days = days_from_civil(
        value->tm_year + 1900,
        (unsigned)(value->tm_mon + 1),
        (unsigned)value->tm_mday);

    return days * TIME_SECONDS_PER_DAY +
           (int64_t)value->tm_hour * 3600 +
           (int64_t)value->tm_min * 60 +
           value->tm_sec;
}

void time_epoch_to_tm(int64_t epoch_seconds, struct tm *out)
{
    if (out == NULL) {
        return;
    }

    int64_t days = epoch_seconds / TIME_SECONDS_PER_DAY;
    int64_t remainder = epoch_seconds % TIME_SECONDS_PER_DAY;

    if (remainder < 0) {
        remainder += TIME_SECONDS_PER_DAY;
        days -= 1;
    }

    int year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civil_from_days(days, &year, &month, &day);

    memset(out, 0, sizeof(*out));
    out->tm_year = year - 1900;
    out->tm_mon = (int)month - 1;
    out->tm_mday = (int)day;
    out->tm_hour = (int)(remainder / 3600);
    out->tm_min = (int)((remainder % 3600) / 60);
    out->tm_sec = (int)(remainder % 60);
    out->tm_wday = (int)((days + 4) % 7);
    if (out->tm_wday < 0) {
        out->tm_wday += 7;
    }
    out->tm_isdst = 0;
}
