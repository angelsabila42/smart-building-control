#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include "esp_err.h"

#define MOUNT_POINT "/sdcard"

#define ACCESS_LOG_FILE MOUNT_POINT "/access_log.csv"
#define ROOM_LOG_FILE MOUNT_POINT "/room_status_log.csv"
#define ADMIN_LOG_FILE MOUNT_POINT "/admin_log.csv"
#define SYSTEM_LOG_FILE MOUNT_POINT "/system_log.csv"
#define LEGACY_LOG_FILE MOUNT_POINT "/cocis_log.csv"

esp_err_t sd_card_init(void);
esp_err_t log_event(const char *event, const char *details);
esp_err_t read_log(const char *path, void (*print_fn)(const char *));
bool sd_card_is_mounted(void);

#endif