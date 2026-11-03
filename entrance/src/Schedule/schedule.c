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
    {7, 0, 21, 0, false}, /* Monday    */
    {7, 0, 21, 0, false}, /* Tuesday   */
    {7, 0, 21, 0, false}, /* Wednesday */
    {7, 0, 21, 0, false}, /* Thursday  */
    {7, 0, 21, 0, false}, /* Friday    */
    {7, 0, 21, 0, false}, /* Saturday  */
    {0, 0,  0, 0, true }, /* Sunday    */
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

bool schedule_set_day(int day, int open_hour, int open_min, int close_hour, int close_min, bool closed)
{
    if (day < 0 || day > 6) {
        ESP_LOGW(TAG, "Invalid day: %d", day);
        return false;
    }
    if (!closed) {
        if (open_hour < 0 || open_hour > 23 || open_min < 0 || open_min > 59 ||
            close_hour < 0 || close_hour > 23 || close_min < 0 || close_min > 59) {
            ESP_LOGW(TAG, "Invalid time values");
            return false;
        }
        int open_total  = open_hour  * 60 + open_min;
        int close_total = close_hour * 60 + close_min;
        if (close_total <= open_total) {
            ESP_LOGW(TAG, "Close time must be after open time");
            return false;
        }
    }
    xSemaphoreTake(sched_mutex, portMAX_DELAY);
    week[day].open_hour  = closed ? 0 : open_hour;
    week[day].open_min   = closed ? 0 : open_min;
    week[day].close_hour = closed ? 0 : close_hour;
    week[day].close_min  = closed ? 0 : close_min;
    week[day].closed     = closed;
    xSemaphoreGive(sched_mutex);

    if (closed) {
        ESP_LOGI(TAG, "%s set to CLOSED", DAY_NAMES[day]);
    } else {
        ESP_LOGI(TAG, "%s set to %02d:%02d - %02d:%02d",
                 DAY_NAMES[day], open_hour, open_min, close_hour, close_min);
    }
    return true;
}

bool schedule_is_open(int day, int current_hour, int current_min)
{
    if (day < 0 || day > 6) return false;
    day_schedule_t s = schedule_get_day(day);
    if (s.closed) return false;
    int now_total   = current_hour  * 60 + current_min;
    int open_total  = s.open_hour   * 60 + s.open_min;
    int close_total = s.close_hour  * 60 + s.close_min;
    return (now_total >= open_total && now_total < close_total);
}
