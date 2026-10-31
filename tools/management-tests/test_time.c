#include <string.h>

#include "test_util.h"

void run_time_tests(void)
{
    uint16_t minutes = 0;
    char buffer[32];

    CHECK(time_parse_hhmm("00:00", &minutes) && minutes == 0);
    CHECK(time_parse_hhmm("07:05", &minutes) && minutes == 425);
    CHECK(time_parse_hhmm("23:59", &minutes) && minutes == 1439);

    CHECK(!time_parse_hhmm("24:00", &minutes));
    CHECK(!time_parse_hhmm("7:00", &minutes));
    CHECK(!time_parse_hhmm("07:60", &minutes));
    CHECK(!time_parse_hhmm("0700", &minutes));
    CHECK(!time_parse_hhmm("07:0a", &minutes));
    CHECK(!time_parse_hhmm(NULL, &minutes));
    CHECK(!time_parse_hhmm("07:00", NULL));

    CHECK(time_parse_hhmm_end("24:00", &minutes) && minutes == 1440);
    CHECK(time_parse_hhmm_end("23:59", &minutes) && minutes == 1439);
    CHECK(!time_parse_hhmm_end("24:01", &minutes));

    time_format_hhmm(0, buffer, sizeof(buffer));
    CHECK_STR(buffer, "00:00");

    time_format_hhmm(425, buffer, sizeof(buffer));
    CHECK_STR(buffer, "07:05");

    time_format_hhmm(1439, buffer, sizeof(buffer));
    CHECK_STR(buffer, "23:59");

    time_format_hhmm(1440, buffer, sizeof(buffer));
    CHECK_STR(buffer, "24:00");

    time_format_hhmm(9999, buffer, sizeof(buffer));
    CHECK_STR(buffer, "24:00");

    struct tm value;

    CHECK(time_parse_datetime("2026-10-30 14:05:09", &value));
    CHECK_INT(value.tm_year + 1900, 2026);
    CHECK_INT(value.tm_mon + 1, 10);
    CHECK_INT(value.tm_mday, 30);
    CHECK_INT(value.tm_hour, 14);
    CHECK_INT(value.tm_min, 5);
    CHECK_INT(value.tm_sec, 9);
    CHECK_INT(value.tm_wday, 5); /* Friday */

    CHECK(time_parse_datetime("2024-02-29 00:00:00", &value));
    CHECK(!time_parse_datetime("2023-02-29 00:00:00", &value));
    CHECK(!time_parse_datetime("2026-13-01 00:00:00", &value));
    CHECK(!time_parse_datetime("2026-10-30 24:00:00", &value));
    CHECK(!time_parse_datetime("2026-10-30 14:05", &value));
    CHECK(!time_parse_datetime("2026-10-30  14:05:00", &value));
    CHECK(!time_parse_datetime("1999-01-01 00:00:00", &value));

    time_format_datetime(&(struct tm) {
        .tm_year = 126, .tm_mon = 9, .tm_mday = 30,
        .tm_hour = 9, .tm_min = 3, .tm_sec = 7
    }, buffer, sizeof(buffer));
    CHECK_STR(buffer, "2026-10-30 09:03:07");

    CHECK_INT(time_tm_to_epoch(&(struct tm) { .tm_year = 70, .tm_mon = 0, .tm_mday = 1 }), 0);
    CHECK_INT(time_tm_to_epoch(&(struct tm) {
        .tm_year = 100, .tm_mon = 0, .tm_mday = 1
    }), 946684800);

    struct tm back;

    time_epoch_to_tm(946684800, &back);
    CHECK_INT(back.tm_year + 1900, 2000);
    CHECK_INT(back.tm_mon + 1, 1);
    CHECK_INT(back.tm_mday, 1);
    CHECK_INT(back.tm_wday, 6); /* Saturday */

    const struct tm source = tm_at(2026, 10, 30, 14, 5);
    const int64_t epoch = time_tm_to_epoch(&source);

    time_epoch_to_tm(epoch, &back);
    CHECK_INT(back.tm_year, source.tm_year);
    CHECK_INT(back.tm_mon, source.tm_mon);
    CHECK_INT(back.tm_mday, source.tm_mday);
    CHECK_INT(back.tm_hour, source.tm_hour);
    CHECK_INT(back.tm_min, source.tm_min);
    CHECK_INT(back.tm_wday, source.tm_wday);

    CHECK(time_epoch_is_plausible(epoch));
    CHECK(!time_epoch_is_plausible(0));
    CHECK(!time_epoch_is_plausible(123456789));

    CHECK_INT(time_day_index(0), 0);
    CHECK_INT(time_day_index(7), 0);
    CHECK_INT(time_day_index(-1), 6);
    CHECK_STR(time_day_short_name(5), "Fri");
    CHECK_STR(time_day_long_name(5), "Friday");

    int day = -1;

    CHECK(time_parse_day("mon", &day) && day == 1);
    CHECK(time_parse_day("MONDAY", &day) && day == 1);
    CHECK(time_parse_day("Sun", &day) && day == 0);
    CHECK(!time_parse_day("mondayy", &day));
    CHECK(!time_parse_day("", &day));
    CHECK(!time_parse_day(NULL, &day));
}
