#include "schedule.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SCHEDULE";

static const char *DAY_NAMES[7] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
};

/* Default: Mon-Sat 07:00-21:00, Sunday closed */
static day_schedule_t week[7] = {
    {7, 21, false}, /* Monday    */
    {7, 21, false}, /* Tuesday   */
    {7, 21, false}, /* Wednesday */
    {7, 21, false}, /* Thursday  */
    {7, 21, false}, /* Friday    */
    {7, 21, false}, /* Saturday  */
    {0,  0, true }, /* Sunday    */
};

static SemaphoreHandle_t sched_mutex = NULL;

void schedule_init(void)
{
    if (sched_mutex == NULL) {
        sched_mutex = xSemaphoreCreateMutex();
    }
    ESP_LOGI(TAG, "Schedule initialised (Mon-Sat 07:00-21:00, Sunday closed)");
}

day_schedule_t schedule_get_day(int day)
{
    day_schedule_t copy;
    xSemaphoreTake(sched_mutex, portMAX_DELAY);
    copy = week[day];
    xSemaphoreGive(sched_mutex);
    return copy;
}

bool schedule_set_day(int day, int open_hour, int close_hour, bool closed)
{
    if (day < 0 || day > 6) {
        ESP_LOGW(TAG, "Invalid day: %d", day);
        return false;
    }
    if (!closed && (open_hour < 0 || open_hour > 23 ||
                    close_hour < 0 || close_hour > 23 ||
                    close_hour <= open_hour)) {
        ESP_LOGW(TAG, "Invalid hours: open=%d close=%d", open_hour, close_hour);
        return false;
    }
    xSemaphoreTake(sched_mutex, portMAX_DELAY);
    week[day].open_hour  = closed ? 0 : open_hour;
    week[day].close_hour = closed ? 0 : close_hour;
    week[day].closed     = closed;
    xSemaphoreGive(sched_mutex);

    if (closed) {
        ESP_LOGI(TAG, "%s set to CLOSED", DAY_NAMES[day]);
    } else {
        ESP_LOGI(TAG, "%s set to %02d:00 - %02d:00", DAY_NAMES[day], open_hour, close_hour);
    }
    return true;
}

bool schedule_is_open(int day, int current_hour)
{
    if (day < 0 || day > 6) return false;
    day_schedule_t s = schedule_get_day(day);
    if (s.closed) return false;
    return (current_hour >= s.open_hour && current_hour < s.close_hour);
}
