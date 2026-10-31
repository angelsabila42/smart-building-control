#include <string.h>

#include "test_util.h"

static access_event_t make_event(const char *uid, bool allowed, int64_t timestamp)
{
    access_event_t event;

    memset(&event, 0, sizeof(event));
    snprintf(event.uid, sizeof(event.uid), "%s", uid);
    event.direction = 'I';
    event.allowed = allowed;
    event.timestamp = timestamp;
    snprintf(event.reason,
             sizeof(event.reason),
             "%s",
             allowed ? "inside schedule window" : "outside scheduled hours");

    return event;
}

static void add_event(access_events_t *events, const char *uid, bool allowed, int64_t timestamp)
{
    access_event_t event = make_event(uid, allowed, timestamp);

    access_events_add(events, &event);
}

void run_events_tests(void)
{
    access_events_t events;

    access_events_init(&events);
    CHECK_INT(access_events_count(&events), 0);

    add_event(&events, "A1", true, 1);
    add_event(&events, "B2", false, 2);
    add_event(&events, "C3", true, 3);

    CHECK_INT(access_events_count(&events), 3);

    access_event_t recent[8];
    const size_t copied = access_events_recent(&events, recent, 8);

    CHECK_INT(copied, 3);
    CHECK_STR(recent[0].uid, "C3");
    CHECK_STR(recent[1].uid, "B2");
    CHECK_STR(recent[2].uid, "A1");
    CHECK(recent[0].allowed);
    CHECK(!recent[1].allowed);

    char text[1024];

    access_events_format_recent(&events, 2, text, sizeof(text));
    CHECK_CONTAINS(text, "Recent access events (2):");
    CHECK_CONTAINS(text, "C3");
    CHECK_CONTAINS(text, "B2");
    CHECK(!strstr(text, "A1"));

    /* Ring buffer wraps and keeps the newest entries. */
    for (int i = 0; i < 40; i++) {
        char uid[8];

        snprintf(uid, sizeof(uid), "U%d", i);
        add_event(&events, uid, i % 2 == 0, i);
    }

    CHECK_INT(access_events_count(&events), ACCESS_EVENTS_CAPACITY);

    const size_t wrapped = access_events_recent(&events, recent, 2);

    CHECK_INT(wrapped, 2);
    CHECK_STR(recent[0].uid, "U39");
    CHECK_STR(recent[1].uid, "U38");

    /* Formatting works when empty. */
    access_events_init(&events);
    access_events_format_recent(&events, 5, text, sizeof(text));
    CHECK_CONTAINS(text, "Recent access events (0):");
    CHECK_CONTAINS(text, "none recorded");
}
