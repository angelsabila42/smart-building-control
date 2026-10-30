#ifndef MANAGEMENT_SERIAL_MENU_H
#define MANAGEMENT_SERIAL_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "management/access_events.h"
#include "management/access_schedule.h"
#include "management/admin_auth.h"
#include "management/system_status.h"

/*
 * Serial management interface.
 *
 * The menu core is transport agnostic: it only writes text through a callback
 * and reads the clock through callbacks, so the exact same command handling is
 * used on the ESP32 (UART console) and in the host unit tests.
 */
typedef struct {
    void *context;

    /* Output sink, usually UART0 on the ESP32. */
    void (*write)(void *context, const char *text);

    /* Called after a protected configuration change; return false on failure. */
    bool (*save_config)(void *context);

    /* Optional audit sink (SD-card log). */
    void (*audit)(void *context, const char *event, const char *details);

    /* Wall clock for displayed time and schedule decisions. */
    bool (*get_wall_clock)(void *context, struct tm *out);
    bool (*set_wall_clock)(void *context, const struct tm *value);
    int64_t (*now_epoch)(void *context);

    /* Monotonic seconds for session timeouts and lockouts. */
    int64_t (*monotonic_seconds)(void *context);

    /* Called before 'status' so heap/room values are fresh. */
    void (*refresh_status)(void *context);

    access_schedule_t *schedule;
    admin_auth_t *auth;
    access_events_t *events;
    system_status_t *status;
} serial_menu_config_t;

/* Reads the current configuration into a menu instance. */
bool serial_menu_init(const serial_menu_config_t *config);

void serial_menu_print_banner(void);
void serial_menu_print_prompt(void);
void serial_menu_print_help(void);

/* Executes one command line (without the trailing newline). */
void serial_menu_execute(const char *line);

#endif /* MANAGEMENT_SERIAL_MENU_H */
