#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t sd_card_init(void);
esp_err_t log_event(const char *event, const char *details);
bool sd_card_is_mounted(void);

#endif