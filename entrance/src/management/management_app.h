#ifndef MANAGEMENT_APP_H
#define MANAGEMENT_APP_H

#include <stdbool.h>

/*
 * ESP32 adapter for the management interface.
 *
 * management_app_start() initialises NVS, restores the schedule and credentials,
 * starts the UART console and the system-status snapshot.  Other modules should
 * use the small integration hooks below instead of touching menu internals.
 */

void management_app_start(void);

/*
 * Applies the access policy to a card presentation.
 * direction: 'I' = entry, 'O' = exit, anything else is treated as entry.
 * Returns true when the decision allows the door to open.
 */
bool management_access_check(const char *uid, char direction);

/*
 * Same as management_access_check(), but also takes the door allowlist result
 * (for example access_control_check() in Accesscontrol/).  Entry requires both
 * an authorized tag and a schedule that allows entry; egress is never blocked
 * by the schedule.  The recorded event uses the combined decision, so the
 * serial history, the SD audit trail and the door behaviour cannot disagree.
 */
bool management_access_check_authorized(const char *uid,
                                        char direction,
                                        bool authorized);

/* Convenience wrapper for the current single-reader entrance setup. */
bool management_on_card_seen(const char *uid);

/* True once management_app_start() has completed. */
bool management_app_ready(void);

/* Refreshes heap/clock/schedule fields shown by 'status'. */
void management_app_refresh(void);

/* Integration hooks for the storage and networking owners. */
void management_app_set_storage(bool mounted);
void management_app_set_connectivity(bool wifi_connected, bool mqtt_connected);
void management_app_update_room(int room_index, bool occupied, bool light_on);

/* Optional persistent audit sink (for example the SD-card log). */
void management_app_set_audit_hook(void (*hook)(const char *event, const char *details));

#endif /* MANAGEMENT_APP_H */
