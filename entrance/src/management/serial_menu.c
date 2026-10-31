#include "management/serial_menu.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "management/time_utils.h"

#define MENU_LINE_MAX 192
#define MENU_MAX_TOKENS 10
#define MENU_TEXT_BUFFER 4096

static serial_menu_config_t menu;
static bool menu_ready = false;
static char menu_scratch[MENU_TEXT_BUFFER];

static void menu_write(const char *text)
{
    if (menu.write != NULL) {
        menu.write(menu.context, text);
    }
}

static void menu_line(const char *text)
{
    menu_write(text);
    menu_write("\n");
}

static void menu_format(const char *format, ...)
{
    char buffer[512];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    menu_line(buffer);
}

static bool text_equals(const char *left, const char *right)
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

static bool is_space(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static size_t tokenize(char *line, char *tokens[], size_t max_tokens)
{
    size_t count = 0;
    char *cursor = line;

    while (*cursor != '\0') {
        while (is_space(*cursor)) {
            cursor++;
        }

        if (*cursor == '\0') {
            break;
        }

        if (count < max_tokens) {
            tokens[count++] = cursor;
        }

        while (*cursor != '\0' && !is_space(*cursor)) {
            cursor++;
        }

        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }

    return count;
}

static int64_t menu_now_epoch(void)
{
    return menu.now_epoch != NULL ? menu.now_epoch(menu.context) : 0;
}

static int64_t menu_now_monotonic(void)
{
    if (menu.monotonic_seconds != NULL) {
        return menu.monotonic_seconds(menu.context);
    }

    return menu_now_epoch();
}

static bool menu_wall_clock(struct tm *out)
{
    return menu.get_wall_clock != NULL && menu.get_wall_clock(menu.context, out);
}

static bool menu_session_valid(void)
{
    return menu.auth != NULL && admin_auth_session_valid(menu.auth, menu_now_monotonic());
}

static bool menu_require_admin(void)
{
    if (menu_session_valid()) {
        return true;
    }

    menu_line("ERR: authentication required; use 'login <user> <password>'");
    return false;
}

static void menu_session_touch(void)
{
    if (menu.auth != NULL) {
        admin_auth_touch(menu.auth, menu_now_monotonic());
    }
}

static const char *menu_admin_name(void)
{
    if (menu.auth != NULL && menu.auth->credentials.username[0] != '\0') {
        return menu.auth->credentials.username;
    }

    return "admin";
}

static void menu_save_and_audit(const char *event, const char *details)
{
    if (menu.save_config != NULL && !menu.save_config(menu.context)) {
        menu_line("WARN: change applied in RAM but could not be persisted");
    }

    if (menu.audit != NULL) {
        menu.audit(menu.context, event, details);
    }
}

static void menu_audit(const char *event, const char *details)
{
    if (menu.audit != NULL) {
        menu.audit(menu.context, event, details);
    }
}

static void print_clock(void)
{
    struct tm now;
    const bool valid = menu_wall_clock(&now);

    if (!valid) {
        menu_line("Clock: NOT SET");
        menu_line("The access schedule fails closed until the clock is set.");
        menu_line("Set it with: time set YYYY-MM-DD HH:MM:SS");
        return;
    }

    char stamp[24];

    time_format_datetime(&now, stamp, sizeof(stamp));
    menu_format("Clock: %s (%s)", stamp, time_epoch_is_plausible(menu_now_epoch()) ? "valid" : "unset");
    menu_format("Day: %s", time_day_long_name(time_day_index(now.tm_wday)));
    menu_line("Set it with: time set YYYY-MM-DD HH:MM:SS");
}

static void command_help(void)
{
    menu_write(
        "Commands:\n"
        "  help                             Show this list\n"
        "  status                           System, storage and room status\n"
        "  events [count]                   Recent access events (default 10)\n"
        "  schedule                         Show the access schedule\n"
        "  time                             Show the device clock\n"
        "  whoami                           Show the administrator session\n"
        "  login <user> <password>          Authenticate for configuration\n"
        "  logout                           End the administrator session\n"
        "  passwd <old> <new>               Change the administrator password\n"
        "  schedule set <day> <start> <end> [<start2> <end2>]\n"
        "  schedule clear <day>             Remove a day's windows\n"
        "  schedule enable|disable <day>    Enable or close a day\n"
        "  schedule default                 Restore Mon-Fri 07:00-21:00, Sat 08:00-13:00\n"
        "  schedule on|off                  Enable or disable enforcement\n"
        "  override on [minutes]            Temporarily bypass the schedule\n"
        "  override off                     Cancel the temporary override\n"
        "  time set YYYY-MM-DD HH:MM:SS     Set the device clock\n"
        "Days: sun mon tue wed thu fri sat. Times: HH:MM (00:00-24:00, end may be 24:00).\n"
        "Commands marked protected below require 'login' first.\n"
        "Protected: passwd, schedule changes, override, time set.\n");
}

static void command_status(void)
{
    if (menu.refresh_status != NULL) {
        menu.refresh_status(menu.context);
    }

    const int64_t now = menu_now_epoch();

    if (menu.status != NULL) {
        if (menu.schedule != NULL) {
            menu.status->schedule_enabled = menu.schedule->schedule_enabled;
            menu.status->override_active =
                access_schedule_override_active(menu.schedule, now);
        }

        system_status_expire_rooms(menu.status, now, 120);
        system_status_format(menu.status, now, menu_scratch, sizeof(menu_scratch));
        menu_line(menu_scratch);
    }

    if (menu_session_valid()) {
        menu_format("Admin session: active (%llds idle timeout remaining)",
                    (long long)admin_auth_session_remaining(menu.auth, menu_now_monotonic()));
    } else {
        menu_line("Admin session: not authenticated");
    }
}

static void command_events(int token_count, char *tokens[])
{
    size_t limit = 10;

    if (token_count >= 2) {
        char *end = NULL;
        const long requested = strtol(tokens[1], &end, 10);

        if (end == tokens[1] || *end != '\0' ||
            requested < 1 || requested > ACCESS_EVENTS_CAPACITY) {
            menu_format("ERR: count must be 1-%d", ACCESS_EVENTS_CAPACITY);
            return;
        }

        limit = (size_t)requested;
    }

    if (menu.events == NULL) {
        menu_line("ERR: event history is unavailable");
        return;
    }

    access_events_format_recent(menu.events, limit, menu_scratch, sizeof(menu_scratch));
    menu_line(menu_scratch);
}

static void print_schedule(void)
{
    if (menu.schedule == NULL) {
        menu_line("ERR: schedule is unavailable");
        return;
    }

    access_schedule_format(menu.schedule, menu_now_epoch(), menu_scratch, sizeof(menu_scratch));
    menu_line(menu_scratch);
    if (!menu.schedule->schedule_enabled) {
        menu_line("WARN: enforcement is OFF; entry is allowed at any time");
    }
}

static bool parse_window_pair(char *start_text,
                              char *end_text,
                              access_window_t *window)
{
    uint16_t start = 0;
    uint16_t end = 0;

    if (!time_parse_hhmm(start_text, &start)) {
        menu_format("ERR: invalid start time '%s' (use HH:MM, 00:00-23:59)", start_text);
        return false;
    }

    if (!time_parse_hhmm_end(end_text, &end)) {
        menu_format("ERR: invalid end time '%s' (use HH:MM, 00:01-24:00)", end_text);
        return false;
    }

    if (start >= end) {
        menu_format("ERR: start '%s' must be before end '%s'", start_text, end_text);
        return false;
    }

    window->start = start;
    window->end = end;
    return true;
}

static void command_schedule_set(int token_count, char *tokens[])
{
    if (token_count != 5 && token_count != 7) {
        menu_line("Usage: schedule set <day> <start> <end> [<start2> <end2>]");
        return;
    }

    int day = 0;

    if (!time_parse_day(tokens[2], &day)) {
        menu_line("ERR: invalid day (use sun mon tue wed thu fri sat)");
        return;
    }

    access_window_t windows[ACCESS_SCHEDULE_MAX_WINDOWS];
    memset(windows, 0, sizeof(windows));

    if (!parse_window_pair(tokens[3], tokens[4], &windows[0])) {
        return;
    }

    size_t window_count = 1;

    if (token_count == 7) {
        if (!parse_window_pair(tokens[5], tokens[6], &windows[1])) {
            return;
        }

        window_count = 2;
    }

    char error[96];
    const access_schedule_error_t result = access_schedule_set_day(
        menu.schedule, day, windows, window_count, error, sizeof(error));

    if (result != ACCESS_SCHEDULE_OK) {
        menu_format("ERR: schedule rejected: %s", error);
        return;
    }

    char line[96];

    access_schedule_touch(menu.schedule, menu_now_epoch(), menu_admin_name());
    access_schedule_format_day(menu.schedule, day, line, sizeof(line));
    menu_format("OK: %s (day enabled)", line);

    char details[160];

    snprintf(details, sizeof(details), "%s by %s", line, menu_admin_name());
    menu_save_and_audit("SCHEDULE_UPDATE", details);
}

static void command_schedule_day(int token_count, char *tokens[], const char *action)
{
    if (token_count != 3) {
        menu_format("Usage: schedule %s <day>", action);
        return;
    }

    int day = 0;

    if (!time_parse_day(tokens[2], &day)) {
        menu_line("ERR: invalid day (use sun mon tue wed thu fri sat)");
        return;
    }

    access_schedule_error_t result = ACCESS_SCHEDULE_OK;

    if (strcmp(action, "clear") == 0) {
        result = access_schedule_clear_day(menu.schedule, day);
    } else if (strcmp(action, "enable") == 0) {
        result = access_schedule_set_day_enabled(menu.schedule, day, true);
    } else {
        result = access_schedule_set_day_enabled(menu.schedule, day, false);
    }

    if (result == ACCESS_SCHEDULE_ERR_NO_ACTIVE_DAYS) {
        menu_line("ERR: that would leave no active day; use 'schedule off' to stop enforcement");
        return;
    }

    if (result == ACCESS_SCHEDULE_ERR_NO_WINDOWS) {
        menu_line("ERR: this day has no windows; use 'schedule set <day> ...' first");
        return;
    }

    if (result != ACCESS_SCHEDULE_OK) {
        menu_line("ERR: could not update the day");
        return;
    }

    char line[96];

    access_schedule_touch(menu.schedule, menu_now_epoch(), menu_admin_name());
    access_schedule_format_day(menu.schedule, day, line, sizeof(line));
    menu_format("OK: %s", line);

    char details[160];

    snprintf(details, sizeof(details), "%s %s by %s", action, tokens[2], menu_admin_name());
    menu_save_and_audit("SCHEDULE_UPDATE", details);
}

static void command_schedule_switch(int token_count, char *tokens[], bool enable)
{
    (void)tokens;

    if (token_count != 2) {
        menu_line(enable ? "Usage: schedule on" : "Usage: schedule off");
        return;
    }

    if (menu.schedule == NULL) {
        menu_line("ERR: schedule is unavailable");
        return;
    }

    menu.schedule->schedule_enabled = enable;
    access_schedule_touch(menu.schedule, menu_now_epoch(), menu_admin_name());

    if (enable) {
        char error[96];
        const access_schedule_error_t result =
            access_schedule_validate(menu.schedule, error, sizeof(error));

        if (result != ACCESS_SCHEDULE_OK) {
            menu.schedule->schedule_enabled = false;
            menu_format("ERR: cannot enable schedule: %s", error);
            return;
        }

        menu_line("OK: schedule enforcement ENABLED");
    } else {
        menu_line("OK: schedule enforcement DISABLED (entry allowed at any time)");
    }

    menu_save_and_audit("SCHEDULE_ENFORCEMENT", enable ? "enabled" : "disabled");
}

static void command_schedule_reset(void)
{
    access_schedule_defaults(menu.schedule);
    access_schedule_touch(menu.schedule, menu_now_epoch(), menu_admin_name());
    menu_line("OK: default schedule restored");
    menu_save_and_audit("SCHEDULE_RESET", menu_admin_name());
}

static void command_schedule(int token_count, char *tokens[])
{
    if (token_count == 1 || text_equals(tokens[1], "show")) {
        print_schedule();
        return;
    }

    if (!menu_require_admin()) {
        return;
    }

    menu_session_touch();

    if (text_equals(tokens[1], "set")) {
        command_schedule_set(token_count, tokens);
        return;
    }

    if (text_equals(tokens[1], "clear") ||
        text_equals(tokens[1], "enable") ||
        text_equals(tokens[1], "disable")) {
        command_schedule_day(token_count, tokens, tokens[1]);
        menu_session_touch();
        return;
    }

    if (text_equals(tokens[1], "default")) {
        if (token_count != 2) {
            menu_line("Usage: schedule default");
            return;
        }

        command_schedule_reset();
        menu_session_touch();
        return;
    }

    if (text_equals(tokens[1], "on") || text_equals(tokens[1], "off")) {
        command_schedule_switch(token_count, tokens, text_equals(tokens[1], "on"));
        menu_session_touch();
        return;
    }

    menu_line("ERR: unknown schedule command; type 'help'");
}

static void command_override(int token_count, char *tokens[])
{
    if (token_count < 2 || token_count > 3) {
        menu_line("Usage: override <on [minutes]|off>");
        return;
    }

    if (!menu_require_admin()) {
        return;
    }

    if (text_equals(tokens[1], "off")) {
        if (token_count != 2) {
            menu_line("Usage: override off");
            return;
        }

        access_schedule_set_override(menu.schedule, 0);
        menu_line("OK: override cancelled");
        menu_save_and_audit("OVERRIDE_OFF", menu_admin_name());
        menu_session_touch();
        return;
    }

    if (!text_equals(tokens[1], "on")) {
        menu_line("Usage: override <on [minutes]|off>");
        return;
    }

    long minutes = ACCESS_SCHEDULE_OVERRIDE_DEFAULT_MINUTES;

    if (token_count == 3) {
        char *end = NULL;
        minutes = strtol(tokens[2], &end, 10);

        if (end == tokens[2] || *end != '\0' || minutes < 1 || minutes > 1440) {
            menu_line("ERR: minutes must be 1-1440");
            return;
        }
    }

    const int64_t now = menu_now_epoch();

    if (!time_epoch_is_plausible(now)) {
        menu_line("ERR: set the clock before using an override");
        return;
    }

    const int64_t until = now + minutes * 60;

    access_schedule_set_override(menu.schedule, until);

    struct tm expiry;
    char stamp[24];

    time_epoch_to_tm(until, &expiry);
    time_format_datetime(&expiry, stamp, sizeof(stamp));
    menu_format("OK: override active until %s", stamp);

    char details[128];

    snprintf(details, sizeof(details), "until %s by %s", stamp, menu_admin_name());
    menu_save_and_audit("OVERRIDE_ON", details);
    menu_session_touch();
}

static void command_time(int token_count, char *tokens[])
{
    if (token_count == 1) {
        print_clock();
        return;
    }

    if (!text_equals(tokens[1], "set")) {
        menu_line("Usage: time [set YYYY-MM-DD HH:MM:SS]");
        return;
    }

    if (!menu_require_admin()) {
        return;
    }

    if (token_count != 4) {
        menu_line("Usage: time set YYYY-MM-DD HH:MM:SS");
        return;
    }

    char value[32];

    snprintf(value, sizeof(value), "%s %s", tokens[2], tokens[3]);

    struct tm parsed;

    if (!time_parse_datetime(value, &parsed)) {
        menu_format("ERR: invalid date/time '%s' (use YYYY-MM-DD HH:MM:SS)", value);
        return;
    }

    if (menu.set_wall_clock == NULL || !menu.set_wall_clock(menu.context, &parsed)) {
        menu_line("ERR: the clock could not be updated");
        return;
    }

    char stamp[24];

    time_format_datetime(&parsed, stamp, sizeof(stamp));
    menu_format("OK: clock set to %s", stamp);

    if (menu.status != NULL) {
        menu.status->clock_valid = time_epoch_is_plausible(time_tm_to_epoch(&parsed));
    }

    menu_audit("CLOCK_SET", stamp);
    menu_session_touch();
}

static void command_login(int token_count, char *tokens[])
{
    if (token_count != 3) {
        menu_line("Usage: login <user> <password>");
        return;
    }

    const int64_t now = menu_now_monotonic();

    if (menu.auth == NULL) {
        menu_line("ERR: authentication is unavailable");
        return;
    }

    const int64_t locked = admin_auth_lockout_remaining(menu.auth, now);

    if (locked > 0) {
        menu_format("ERR: too many failed attempts; try again in %llds", (long long)locked);
        return;
    }

    const admin_auth_result_t result =
        admin_auth_login(menu.auth, tokens[1], tokens[2], now);

    if (result == ADMIN_AUTH_OK) {
        menu_format("OK: logged in as %s", menu_admin_name());

        if (admin_auth_uses_default_password(menu.auth)) {
            menu_line("WARN: default password is active; change it with 'passwd <old> <new>'");
        }

        menu_audit("LOGIN", menu_admin_name());
        return;
    }

    if (result == ADMIN_AUTH_ERR_LOCKED) {
        menu_format("ERR: locked out for %llds after %d failed attempts",
                    (long long)admin_auth_lockout_remaining(menu.auth, now),
                    ADMIN_AUTH_MAX_FAILED_ATTEMPTS);
        menu_audit("LOGIN_LOCKED", tokens[1]);
        return;
    }

    if (result == ADMIN_AUTH_ERR_CREDENTIALS) {
        const int remaining = ADMIN_AUTH_MAX_FAILED_ATTEMPTS - menu.auth->failed_attempts;

        menu_format("ERR: invalid credentials (%d attempt%s left)",
                    remaining,
                    remaining == 1 ? "" : "s");
        menu_audit("LOGIN_FAILED", tokens[1]);
        return;
    }

    menu_line("ERR: login failed");
}

static void command_passwd(int token_count, char *tokens[])
{
    if (token_count != 3) {
        menu_line("Usage: passwd <old> <new>");
        return;
    }

    if (!menu_require_admin()) {
        return;
    }

    const admin_auth_result_t result = admin_auth_change_password(
        menu.auth, tokens[1], tokens[2], menu_now_monotonic());

    switch (result) {
        case ADMIN_AUTH_OK:
            menu_line("OK: administrator password changed");
            menu_save_and_audit("PASSWORD_CHANGE", menu_admin_name());
            break;
        case ADMIN_AUTH_ERR_CREDENTIALS:
            menu_line("ERR: current password is incorrect");
            break;
        case ADMIN_AUTH_ERR_WEAK_PASSWORD:
            menu_line("ERR: new password must be 8-64 characters with a letter and a digit, and differ from the old one");
            break;
        case ADMIN_AUTH_ERR_NOT_AUTHENTICATED:
            menu_line("ERR: authentication required");
            break;
        default:
            menu_line("ERR: password change failed");
            break;
    }

    menu_session_touch();
}

static void command_whoami(void)
{
    if (!menu_session_valid()) {
        menu_line("Not authenticated. Use 'login <user> <password>'.");
        return;
    }

    const int64_t remaining =
        admin_auth_session_remaining(menu.auth, menu_now_monotonic());

    menu_format("Authenticated as %s; idle timeout in %llds",
                menu_admin_name(),
                (long long)remaining);

    if (admin_auth_uses_default_password(menu.auth)) {
        menu_line("WARN: default password is active");
    }
}

bool serial_menu_init(const serial_menu_config_t *config)
{
    if (config == NULL || config->write == NULL || config->schedule == NULL ||
        config->auth == NULL || config->status == NULL) {
        return false;
    }

    menu = *config;
    menu_ready = true;

    return true;
}

void serial_menu_print_banner(void)
{
    if (!menu_ready) {
        return;
    }

    menu_line("");
    menu_line("CoCIS Smart Building - Main Entrance Terminal");
    menu_format("Management interface | firmware %s", menu.status->firmware);
    menu_line("Type 'help' for commands.");

    if (admin_auth_uses_default_password(menu.auth)) {
        menu_line("WARN: default administrator password is active. Change it with 'passwd <old> <new>'.");
    }

    menu_line("");
}

void serial_menu_print_prompt(void)
{
    if (menu_ready) {
        menu_write("cocis> ");
    }
}

void serial_menu_print_help(void)
{
    if (menu_ready) {
        command_help();
    }
}

void serial_menu_execute(const char *line)
{
    if (!menu_ready || line == NULL) {
        return;
    }

    char buffer[MENU_LINE_MAX];

    snprintf(buffer, sizeof(buffer), "%s", line);

    char *tokens[MENU_MAX_TOKENS];
    const size_t token_count = tokenize(buffer, tokens, MENU_MAX_TOKENS);

    if (token_count == 0) {
        return;
    }

    const char *command = tokens[0];

    if (text_equals(command, "help") || text_equals(command, "?") || text_equals(command, "h")) {
        command_help();
        return;
    }

    if (text_equals(command, "status") || text_equals(command, "stat")) {
        command_status();
        return;
    }

    if (text_equals(command, "events")) {
        command_events((int)token_count, tokens);
        return;
    }

    if (text_equals(command, "schedule")) {
        command_schedule((int)token_count, tokens);
        return;
    }

    if (text_equals(command, "time")) {
        command_time((int)token_count, tokens);
        return;
    }

    if (text_equals(command, "whoami")) {
        command_whoami();
        return;
    }

    if (text_equals(command, "login")) {
        command_login((int)token_count, tokens);
        return;
    }

    if (text_equals(command, "logout")) {
        admin_auth_logout(menu.auth);
        menu_line("OK: logged out");
        menu_audit("LOGOUT", menu_admin_name());
        return;
    }

    if (text_equals(command, "passwd")) {
        command_passwd((int)token_count, tokens);
        return;
    }

    if (text_equals(command, "override")) {
        command_override((int)token_count, tokens);
        return;
    }

    menu_format("ERR: unknown command '%s'; type 'help'", command);
}
