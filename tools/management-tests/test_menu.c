#include <stdio.h>
#include <string.h>

#include "management/serial_menu.h"
#include "test_util.h"

static access_schedule_t schedule;
static admin_auth_t auth;
static access_events_t events;
static system_status_t status;

static char output[32768];
static size_t output_length = 0;

static struct tm fake_wall_clock;
static int64_t fake_epoch = 0;
static int64_t fake_monotonic = 1000;
static bool fake_clock_valid = true;
static bool fake_save_result = true;
static int fake_save_calls = 0;
static char last_audit_event[64];
static char last_audit_details[256];

static void clear_output(void)
{
    output[0] = '\0';
    output_length = 0;
}

static void fake_write(void *context, const char *text)
{
    (void)context;

    const size_t length = strlen(text);

    if (output_length + length + 1 >= sizeof(output)) {
        return;
    }

    memcpy(output + output_length, text, length);
    output_length += length;
    output[output_length] = '\0';
}

static bool fake_get_wall_clock(void *context, struct tm *out)
{
    (void)context;

    if (!fake_clock_valid) {
        return false;
    }

    *out = fake_wall_clock;
    return true;
}

static bool fake_set_wall_clock(void *context, const struct tm *value)
{
    (void)context;

    fake_wall_clock = *value;
    fake_epoch = time_tm_to_epoch(value);
    fake_clock_valid = true;
    return true;
}

static int64_t fake_now(void *context)
{
    (void)context;
    return fake_epoch;
}

static int64_t fake_mono(void *context)
{
    (void)context;
    return fake_monotonic;
}

static bool fake_save(void *context)
{
    (void)context;
    fake_save_calls++;
    return fake_save_result;
}

static void fake_audit(void *context, const char *event, const char *details)
{
    (void)context;

    snprintf(last_audit_event, sizeof(last_audit_event), "%s", event);
    snprintf(last_audit_details, sizeof(last_audit_details), "%s", details);
}

static void fake_refresh(void *context)
{
    (void)context;
    status.free_heap = 4242;
    status.min_free_heap = 2048;
}

static void menu_setup(void)
{
    access_schedule_defaults(&schedule);
    access_events_init(&events);
    system_status_init(&status, "test-fw");

    memset(&auth, 0, sizeof(auth));
    admin_auth_init(&auth, 1000);

    fake_wall_clock = tm_at(2026, 10, 30, 10, 0);
    fake_epoch = time_tm_to_epoch(&fake_wall_clock);
    fake_monotonic = 1000;
    fake_clock_valid = true;
    fake_save_result = true;
    fake_save_calls = 0;
    last_audit_event[0] = '\0';
    last_audit_details[0] = '\0';

    clear_output();

    serial_menu_config_t config = {
        .context = NULL,
        .write = fake_write,
        .save_config = fake_save,
        .audit = fake_audit,
        .get_wall_clock = fake_get_wall_clock,
        .set_wall_clock = fake_set_wall_clock,
        .now_epoch = fake_now,
        .monotonic_seconds = fake_mono,
        .refresh_status = fake_refresh,
        .schedule = &schedule,
        .auth = &auth,
        .events = &events,
        .status = &status,
    };

    CHECK(serial_menu_init(&config));
}

static const char *run(const char *command)
{
    clear_output();
    serial_menu_execute(command);
    return output;
}

static void login(void)
{
    CHECK_CONTAINS(run("login admin ChangeMe123"), "OK: logged in as admin");
}

void run_menu_tests(void)
{
    menu_setup();

    CHECK_CONTAINS(run("status"), "Firmware: test-fw");
    CHECK_CONTAINS(output, "Heap: 4242 bytes free");
    CHECK_CONTAINS(output, "Admin session: not authenticated");

    CHECK_CONTAINS(run("help"), "schedule set <day> <start> <end>");
    CHECK_CONTAINS(output, "Protected:");
    CHECK_CONTAINS(run("?"), "Commands:");
    CHECK_CONTAINS(run("bogus"), "ERR: unknown command 'bogus'");
    CHECK_STR(run(""), "");

    /* Monitoring commands are public. */
    CHECK_CONTAINS(run("schedule"), "Mon 07:00-21:00");
    CHECK_CONTAINS(run("time"), "2026-10-30 10:00:00");
    CHECK_CONTAINS(run("events"), "none recorded");
    CHECK_CONTAINS(run("whoami"), "Not authenticated");

    /* Protected changes require a session. */
    CHECK_CONTAINS(run("schedule set mon 08:00 09:00"), "ERR: authentication required");
    CHECK_INT(schedule.days[1].windows[0].start, 7 * 60);
    CHECK_CONTAINS(run("override on"), "ERR: authentication required");
    CHECK_CONTAINS(run("time set 2026-10-30 11:00:00"), "ERR: authentication required");
    CHECK_CONTAINS(run("passwd ChangeMe123 NewPass123"), "ERR: authentication required");
    CHECK_INT(fake_save_calls, 0);

    /* Invalid credentials. */
    CHECK_CONTAINS(run("login admin wrong-password"), "ERR: invalid credentials");
    CHECK_CONTAINS(run("login admin ChangeMe123"), "OK: logged in as admin");
    CHECK_CONTAINS(run("whoami"), "Authenticated as admin");

    /* Valid schedule change. */
    CHECK_CONTAINS(run("schedule set mon 08:00 10:00"), "OK: Mon 08:00-10:00 (day enabled)");
    CHECK_INT(schedule.days[1].windows[0].start, 8 * 60);
    CHECK_INT(schedule.days[1].windows[0].end, 10 * 60);
    CHECK_INT(fake_save_calls, 1);
    CHECK_STR(last_audit_event, "SCHEDULE_UPDATE");

    /* Invalid times and overlaps are rejected without changing anything. */
    CHECK_CONTAINS(run("schedule set mon 25:00 26:00"), "ERR: invalid start time '25:00'");
    CHECK_INT(schedule.days[1].windows[0].start, 8 * 60);
    CHECK_CONTAINS(run("schedule set mon 07:00 12:00 11:00 13:00"), "ERR: schedule rejected: windows overlap");
    CHECK_CONTAINS(run("schedule set funday 08:00 10:00"), "ERR: invalid day");
    CHECK_CONTAINS(run("schedule set mon 10:00 08:00"), "ERR: start '10:00' must be before end '08:00'");
    CHECK_CONTAINS(run("schedule set mon 08:00"), "Usage: schedule set");

    /* Two windows, boundary behaviour and formatting. */
    CHECK_CONTAINS(run("schedule set mon 08:00 12:00 14:00 16:00"), "OK: Mon 08:00-12:00, 14:00-16:00");
    CHECK_INT(schedule.days[1].window_count, 2);
    CHECK_CONTAINS(run("schedule"), "Mon 08:00-12:00, 14:00-16:00");

    CHECK_CONTAINS(run("schedule disable mon"), "OK: Mon closed");
    CHECK(!schedule.days[1].enabled);
    CHECK_INT(schedule.days[1].window_count, 2);
    CHECK_CONTAINS(run("schedule enable mon"), "OK: Mon 08:00-12:00, 14:00-16:00");
    CHECK(schedule.days[1].enabled);

    /* Enforcement switch and override. */
    CHECK_CONTAINS(run("schedule off"), "OK: schedule enforcement DISABLED");
    CHECK(!schedule.schedule_enabled);
    CHECK_CONTAINS(run("status"), "Schedule: disabled");
    CHECK_CONTAINS(run("schedule on"), "OK: schedule enforcement ENABLED");
    CHECK(schedule.schedule_enabled);

    CHECK_CONTAINS(run("override on 5"), "OK: override active until");
    CHECK_INT(schedule.override_until, fake_epoch + 300);
    CHECK_CONTAINS(run("status"), "Override: ACTIVE");
    CHECK_CONTAINS(run("override off"), "OK: override cancelled");
    CHECK_INT(schedule.override_until, 0);
    CHECK_CONTAINS(run("override on 0"), "ERR: minutes must be 1-1440");
    CHECK_CONTAINS(run("override sideways"), "Usage: override");

    /* Clock changes are validated and audited. */
    CHECK_CONTAINS(run("time set 2026-10-30 11:30:00"), "OK: clock set to 2026-10-30 11:30:00");

    const struct tm expected_clock = tm_at(2026, 10, 30, 11, 30);

    CHECK_INT(fake_epoch, time_tm_to_epoch(&expected_clock));
    CHECK_STR(last_audit_event, "CLOCK_SET");
    CHECK_CONTAINS(run("time"), "2026-10-30 11:30:00");
    CHECK_CONTAINS(run("time set 2026-13-01 10:00:00"), "ERR: invalid date/time");
    CHECK_CONTAINS(run("time set 2026-10-30"), "Usage: time set");

    /* Events command. */
    access_event_t event;

    memset(&event, 0, sizeof(event));
    snprintf(event.uid, sizeof(event.uid), "DE:AD:BE:EF");
    event.direction = 'I';
    event.allowed = true;
    event.timestamp = fake_epoch;
    snprintf(event.reason, sizeof(event.reason), "%s", "inside schedule window");
    access_events_add(&events, &event);

    CHECK_CONTAINS(run("events 5"), "DE:AD:BE:EF");
    CHECK_CONTAINS(output, "GRANTED");
    CHECK_CONTAINS(run("events 0"), "ERR: count must be 1-");
    CHECK_CONTAINS(run("events nope"), "ERR: count must be 1-");

    /* Password change flow. */
    CHECK_CONTAINS(run("passwd ChangeMe123 short"), "ERR: new password must be");
    CHECK_CONTAINS(run("passwd ChangeMe123 NewPass123"), "OK: administrator password changed");
    CHECK_STR(last_audit_event, "PASSWORD_CHANGE");
    CHECK_CONTAINS(run("logout"), "OK: logged out");
    CHECK_CONTAINS(run("whoami"), "Not authenticated");
    CHECK_CONTAINS(run("login admin ChangeMe123"), "ERR: invalid credentials");
    CHECK_CONTAINS(run("login admin NewPass123"), "OK: logged in as admin");

    /* Only active day cannot be removed. */
    menu_setup();
    login();

    for (int day = 0; day < ACCESS_SCHEDULE_DAYS; day++) {
        if (day == 1) {
            continue;
        }

        char command[32];

        snprintf(command, sizeof(command), "schedule clear %s", time_day_short_name(day));
        CHECK_CONTAINS(run(command), "OK:");
    }

    CHECK_CONTAINS(run("schedule clear Mon"), "ERR: that would leave no active day");
    CHECK(schedule.days[1].enabled);

    /* Saved-config failure is reported but not fatal. */
    fake_save_result = false;
    CHECK_CONTAINS(run("schedule set sun 10:00 12:00"), "OK: Sun 10:00-12:00");
    CHECK_CONTAINS(output, "WARN: change applied in RAM but could not be persisted");

    /* Lockout after repeated failures. */
    menu_setup();
    CHECK_CONTAINS(run("login admin bad1"), "ERR: invalid credentials (2 attempts left)");
    CHECK_CONTAINS(run("login admin bad2"), "ERR: invalid credentials (1 attempt left)");
    CHECK_CONTAINS(run("login admin bad3"), "ERR: locked out for 60s");
    CHECK_CONTAINS(run("login admin ChangeMe123"), "ERR: too many failed attempts");
    fake_monotonic += 61;
    CHECK_CONTAINS(run("login admin ChangeMe123"), "OK: logged in as admin");

    /* Clock not set: display fails and time set is still possible. */
    menu_setup();
    fake_clock_valid = false;
    fake_epoch = 0;
    CHECK_CONTAINS(run("time"), "Clock: NOT SET");
    CHECK_CONTAINS(run("status"), "Clock: not set (NOT SET - schedule fails closed)");
    CHECK_CONTAINS(run("override on"), "ERR: authentication required");
    login();
    CHECK_CONTAINS(run("override on"), "ERR: set the clock before using an override");
    CHECK_CONTAINS(run("time set 2026-10-30 09:00:00"), "OK: clock set to 2026-10-30 09:00:00");
    CHECK(fake_clock_valid);
}
