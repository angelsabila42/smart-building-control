#include "admin_auth.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdint.h>

static const char *TAG = "ADMIN_AUTH";

/* djb2 hash — avoids storing the PIN in plain text */
static uint32_t hash_pin(const char *pin)
{
    uint32_t h = 5381;
    int c;
    while ((c = (unsigned char)*pin++)) {
        h = ((h << 5) + h) + c;
    }
    return h;
}

/* Default admin PIN: "1234" — change before deployment */
#define ADMIN_PIN_HASH 0x7c78c98fUL

#define MAX_FAILED_ATTEMPTS 3

static bool              authenticated    = false;
static int               failed_attempts  = 0;
static SemaphoreHandle_t auth_mutex       = NULL;

void admin_auth_init(void)
{
    if (auth_mutex == NULL) {
        auth_mutex = xSemaphoreCreateMutex();
    }
    ESP_LOGI(TAG, "Admin auth initialised");
}

bool admin_login(const char *pin)
{
    if (pin == NULL) return false;

    xSemaphoreTake(auth_mutex, portMAX_DELAY);

    if (failed_attempts >= MAX_FAILED_ATTEMPTS) {
        ESP_LOGW(TAG, "Account locked after %d failed attempts", MAX_FAILED_ATTEMPTS);
        xSemaphoreGive(auth_mutex);
        return false;
    }

    if (hash_pin(pin) == ADMIN_PIN_HASH) {
        authenticated   = true;
        failed_attempts = 0;
        ESP_LOGI(TAG, "Admin authenticated");
        xSemaphoreGive(auth_mutex);
        return true;
    }

    failed_attempts++;
    ESP_LOGW(TAG, "Wrong PIN (%d/%d)", failed_attempts, MAX_FAILED_ATTEMPTS);
    xSemaphoreGive(auth_mutex);
    return false;
}

void admin_logout(void)
{
    xSemaphoreTake(auth_mutex, portMAX_DELAY);
    authenticated = false;
    xSemaphoreGive(auth_mutex);
    ESP_LOGI(TAG, "Admin logged out");
}

bool admin_is_authenticated(void)
{
    xSemaphoreTake(auth_mutex, portMAX_DELAY);
    bool s = authenticated;
    xSemaphoreGive(auth_mutex);
    return s;
}
