#ifndef MANAGEMENT_ACCESS_EVENTS_H
#define MANAGEMENT_ACCESS_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ACCESS_EVENTS_CAPACITY 32
#define ACCESS_EVENT_UID_MAX 24
#define ACCESS_EVENT_REASON_MAX 48

/*
 * In-memory ring buffer of recent access decisions.  Persistent logging to the
 * SD card is done separately; this buffer keeps the serial interface useful
 * even when the card is missing.
 */

typedef struct {
    int64_t timestamp; /* epoch seconds, 0 when the clock was not set */
    char uid[ACCESS_EVENT_UID_MAX];
    char direction; /* 'I' = entry, 'O' = exit, '?' = unknown */
    bool allowed;
    char reason[ACCESS_EVENT_REASON_MAX];
} access_event_t;

typedef struct {
    access_event_t events[ACCESS_EVENTS_CAPACITY];
    size_t head; /* index of the next slot to write */
    size_t count;
    void (*lock)(void *context);
    void (*unlock)(void *context);
    void *lock_context;
} access_events_t;

void access_events_init(access_events_t *events);

/* Optional locking hooks for multi-task use (the console task and RFID task). */
void access_events_set_lock(access_events_t *events,
                            void (*lock)(void *context),
                            void (*unlock)(void *context),
                            void *context);

void access_events_add(access_events_t *events, const access_event_t *event);

size_t access_events_count(access_events_t *events);

/* Copies the newest events first.  Returns the number of entries copied. */
size_t access_events_recent(access_events_t *events,
                            access_event_t *out,
                            size_t max_events);

/* Formats the newest events for the serial interface (no trailing newline). */
void access_events_format_recent(access_events_t *events,
                                 size_t max_events,
                                 char *buffer,
                                 size_t size);

#endif /* MANAGEMENT_ACCESS_EVENTS_H */
