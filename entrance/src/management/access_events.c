#include "management/access_events.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "management/time_utils.h"

void access_events_init(access_events_t *events)
{
    if (events == NULL) {
        return;
    }

    memset(events, 0, sizeof(*events));
}

void access_events_set_lock(access_events_t *events,
                            void (*lock)(void *context),
                            void (*unlock)(void *context),
                            void *context)
{
    if (events == NULL) {
        return;
    }

    events->lock = lock;
    events->unlock = unlock;
    events->lock_context = context;
}

static void events_lock(access_events_t *events)
{
    if (events->lock != NULL) {
        events->lock(events->lock_context);
    }
}

static void events_unlock(access_events_t *events)
{
    if (events->unlock != NULL) {
        events->unlock(events->lock_context);
    }
}

void access_events_add(access_events_t *events, const access_event_t *event)
{
    if (events == NULL || event == NULL) {
        return;
    }

    events_lock(events);

    events->events[events->head] = *event;
    snprintf(events->events[events->head].uid,
             sizeof(events->events[events->head].uid),
             "%s",
             event->uid);
    snprintf(events->events[events->head].reason,
             sizeof(events->events[events->head].reason),
             "%s",
             event->reason);

    events->head = (events->head + 1) % ACCESS_EVENTS_CAPACITY;

    if (events->count < ACCESS_EVENTS_CAPACITY) {
        events->count++;
    }

    events_unlock(events);
}

size_t access_events_count(access_events_t *events)
{
    if (events == NULL) {
        return 0;
    }

    events_lock(events);
    const size_t count = events->count;
    events_unlock(events);

    return count;
}

size_t access_events_recent(access_events_t *events,
                            access_event_t *out,
                            size_t max_events)
{
    if (events == NULL || out == NULL || max_events == 0) {
        return 0;
    }

    events_lock(events);

    size_t copied = 0;
    const size_t limit = events->count < max_events ? events->count : max_events;

    for (size_t i = 0; i < limit; i++) {
        const size_t index =
            (events->head + ACCESS_EVENTS_CAPACITY - 1 - i) % ACCESS_EVENTS_CAPACITY;
        out[copied++] = events->events[index];
    }

    events_unlock(events);

    return copied;
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

void access_events_format_recent(access_events_t *events,
                                 size_t max_events,
                                 char *buffer,
                                 size_t size)
{
    if (events == NULL || buffer == NULL || size == 0) {
        return;
    }

    access_event_t recent[ACCESS_EVENTS_CAPACITY];
    const size_t count = access_events_recent(events, recent, max_events);
    writer_t writer = { buffer, size, 0 };

    writer_append(&writer, "Recent access events (%u):", (unsigned)count);

    if (count == 0) {
        writer_append(&writer, "\n  none recorded");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        char stamp[24] = "clock not set";

        if (recent[i].timestamp > 0) {
            struct tm value;

            time_epoch_to_tm(recent[i].timestamp, &value);
            time_format_datetime(&value, stamp, sizeof(stamp));
        }

        const char *direction = "?";
        if (recent[i].direction == 'I') {
            direction = "IN ";
        } else if (recent[i].direction == 'O') {
            direction = "OUT";
        }

        writer_append(&writer,
                      "\n  %s  %-17s %s  %-7s  %s",
                      stamp,
                      recent[i].uid,
                      direction,
                      recent[i].allowed ? "GRANTED" : "DENIED",
                      recent[i].reason);
    }
}
