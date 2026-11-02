#include "serial_menu.h"
#include "AdminAuth/admin_auth.h"
#include "Schedule/schedule.h"
#include "Accesscontrol/access_control.h"
#include "Sdcard/sd_card.h"
#include "Alarm/alarm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define MENU_UART    UART_NUM_0
#define BUF_SIZE     256
#define UART_TIMEOUT pdMS_TO_TICKS(100)

static const char *TAG = "SERIAL_MENU";

/* ── low-level I/O ───────────────────────────────────────────────────── */

static void u_print(const char *s)
{
    uart_write_bytes(MENU_UART, s, strlen(s));
}

static void u_println(const char *s)
{
    u_print(s);
    u_print("\r\n");
}

static int u_readline(char *buf, int max_len)
{
    int pos = 0;
    uint8_t c;
    while (pos < max_len - 1) {
        if (uart_read_bytes(MENU_UART, &c, 1, UART_TIMEOUT) <= 0) continue;
        uart_write_bytes(MENU_UART, &c, 1);
        if (c == '\r' || c == '\n') { u_print("\r\n"); break; }
        if (c == 0x7F && pos > 0)  { pos--; u_print("\b \b"); continue; }
        buf[pos++] = (char)c;
    }
    buf[pos] = '\0';
    return pos;
}

/* ── box-drawing helpers ─────────────────────────────────────────────── */
/*  Width = 40 inner characters                                           */
#define BOX_W 40

static void box_top(void)    { u_println("+----------------------------------------+"); }
static void box_bottom(void) { u_println("+----------------------------------------+"); }

/* Print a centred title row */
static void box_title(const char *title)
{
    char line[BOX_W + 5];
    int len   = (int)strlen(title);
    int pad_l = (BOX_W - len) / 2;
    int pad_r = BOX_W - len - pad_l;
    snprintf(line, sizeof(line), "|%*s%s%*s|", pad_l, "", title, pad_r, "");
    u_println(line);
}

/* Print a left-aligned menu item row */
static void box_item(const char *item)
{
    char line[BOX_W + 5];
    int len = (int)strlen(item);
    int pad = BOX_W - 1 - len;          /* 1 leading space */
    if (pad < 0) pad = 0;
    snprintf(line, sizeof(line), "| %s%*s|", item, pad, "");
    u_println(line);
}

static void box_divider(void) { u_println("|----------------------------------------|"); }

/* ── main menu ───────────────────────────────────────────────────────── */

static void show_main_menu(void)
{
    u_print("\r\n");
    box_top();
    box_title("CoCIS Smart Control System v1.0");
    box_divider();
    box_item("[1] Building occupancy");
    box_item("[2] Access log");
    box_item("[3] Room status & lighting");
    box_item("[4] Current time & schedule");
    box_item("[5] SD card status");
    box_item("[6] Admin menu  (PIN required)");
    box_item("[0] Refresh menu");
    box_bottom();
    u_print("Choice: ");
}

/* ── admin menu ──────────────────────────────────────────────────────── */

static void show_admin_menu(void)
{
    u_print("\r\n");
    box_top();
    box_title("Admin Menu");
    box_divider();
    box_item("[1] Set access schedule");
    box_item("[2] Add authorized user");
    box_item("[3] Remove authorized user");
    box_item("[4] List authorized users");
    box_item("[5] Flush SD logs now");
    box_item("[6] Alarm status");
    box_item("[0] Back to main menu");
    box_bottom();
    u_print("Choice: ");
}

/* ── main menu handlers ──────────────────────────────────────────────── */

static void handle_occupancy(void)
{
    char line[64];
    snprintf(line, sizeof(line), "Building occupancy: %d person(s)", occupancy_get_current());
    u_println(line);
}

static void handle_access_log(void)
{
    u_println("Access log is stored on the SD card at /sdcard/cocis_log.csv");
    u_println("Mount the SD card and open the file to review entries.");
}

static void handle_room_status(void)
{
    /* Placeholder — room data comes from lecture-room ESP32s via MQTT */
    u_println("Room 1: status unavailable (awaiting MQTT update)");
    u_println("Room 2: status unavailable (awaiting MQTT update)");
}

static void handle_time_schedule(void)
{
    static const char *days[7] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };
    char line[80];
    u_println("Day   Open  Close");
    u_println("----  -----  -----");
    for (int d = 0; d < 7; d++) {
        day_schedule_t s = schedule_get_day(d);
        if (s.closed) {
            snprintf(line, sizeof(line), "%-4s  CLOSED", days[d]);
        } else {
            snprintf(line, sizeof(line), "%-4s  %02d:00  %02d:00", days[d], s.open_hour, s.close_hour);
        }
        u_println(line);
    }
}

static void handle_sd_status(void)
{
    u_println(sd_card_is_mounted() ? "SD card : mounted" : "SD card : not mounted");
}

/* ── admin menu handlers ─────────────────────────────────────────────── */

static void admin_set_schedule(void)
{
    char buf[32];
    char line[80];

    u_println("Select day: 0=Mon 1=Tue 2=Wed 3=Thu 4=Fri 5=Sat 6=Sun");
    u_print("Day (0-6): ");
    u_readline(buf, sizeof(buf));
    int day = atoi(buf);
    if (day < 0 || day > 6) { u_println("Invalid day."); return; }

    u_print("Mark as closed? (y/n): ");
    u_readline(buf, sizeof(buf));
    if (buf[0] == 'y' || buf[0] == 'Y') {
        if (schedule_set_day(day, 0, 0, true)) {
            u_println("Day marked as closed.");
            log_event("SCHEDULE_SET", "day marked closed");
        }
        return;
    }

    u_print("Open hour  (0-23): ");
    u_readline(buf, sizeof(buf));
    int open_h = atoi(buf);

    u_print("Close hour (0-23): ");
    u_readline(buf, sizeof(buf));
    int close_h = atoi(buf);

    if (schedule_set_day(day, open_h, close_h, false)) {
        snprintf(line, sizeof(line), "Schedule updated: day=%d %02d:00-%02d:00", day, open_h, close_h);
        u_println(line);
        log_event("SCHEDULE_SET", line);
    } else {
        u_println("Error: close must be > open, both 0-23.");
    }
}

static void admin_add_user(void)
{
    char buf[32];
    u_print("User ID  : ");
    u_readline(buf, sizeof(buf));
    uint32_t id = (uint32_t)atol(buf);

    u_print("RFID UID : ");
    u_readline(buf, sizeof(buf));
    uint64_t uid = (uint64_t)strtoull(buf, NULL, 10);

    if (user_add(id, uid)) {
        u_println("User added.");
        log_event("USER_ADDED", buf);
    } else {
        u_println("Failed: duplicate UID or user table full.");
    }
}

static void admin_remove_user(void)
{
    char buf[32];
    u_print("RFID UID to remove: ");
    u_readline(buf, sizeof(buf));
    uint64_t uid = (uint64_t)strtoull(buf, NULL, 10);

    if (user_remove(uid)) {
        u_println("User removed.");
        log_event("USER_REMOVED", buf);
    } else {
        u_println("UID not found.");
    }
}

static void admin_list_users(void)
{
    user_t buf[MAX_USERS];
    int count = user_list(buf, MAX_USERS);
    char line[80];
    u_println("  ID       UID                Auth");
    u_println("  -------- ------------------ ----");
    for (int i = 0; i < count; i++) {
        snprintf(line, sizeof(line), "  %-8" PRIu32 " %-18" PRIu64 " %s",
                 buf[i].id, buf[i].rfid_uid,
                 buf[i].authorized ? "yes" : "no");
        u_println(line);
    }
}

static void admin_flush_logs(void)
{
    /* SD card writes are already flushed per-event; this is a confirmation */
    if (sd_card_is_mounted()) {
        log_event("LOG_FLUSH", "manual flush requested");
        u_println("Logs flushed.");
    } else {
        u_println("SD card not mounted — nothing to flush.");
    }
}

static void admin_alarm_control(void)
{
    bool current = alarm_is_active();
    char buf[8];
    char line[64];
    snprintf(line, sizeof(line), "Alarm is currently: %s", current ? "ACTIVE" : "inactive");
    u_println(line);
    u_print(current ? "Clear alarm? (y/n): " : "Trigger alarm? (y/n): ");
    u_readline(buf, sizeof(buf));
    if (buf[0] == 'y' || buf[0] == 'Y') {
        alarm_set(!current);
        u_println(!current ? "Alarm ACTIVATED." : "Alarm cleared.");
    } else {
        u_println("Cancelled.");
    }
}

static void run_admin_menu(void)
{
    char buf[8];
    while (1) {
        show_admin_menu();
        u_readline(buf, sizeof(buf));
        u_print("\r\n");
        switch (buf[0]) {
            case '1': admin_set_schedule();  break;
            case '2': admin_add_user();      break;
            case '3': admin_remove_user();   break;
            case '4': admin_list_users();    break;
            case '5': admin_flush_logs();    break;
            case '6': admin_alarm_control(); break;
            case '0': return;
            default:  u_println("Invalid choice."); break;
        }
    }
}

/* ── main menu entry for [6] ─────────────────────────────────────────── */

static void handle_admin_entry(void)
{
    if (admin_is_authenticated()) {
        run_admin_menu();
        return;
    }
    char pin[64];
    u_print("Enter admin PIN: ");
    u_readline(pin, sizeof(pin));
    if (admin_login(pin)) {
        u_println("Access granted.");
        run_admin_menu();
    } else {
        u_println("Access denied.");
    }
}

/* ── main loop ───────────────────────────────────────────────────────── */

static void serial_menu_task(void *arg)
{
    char buf[8];
    show_main_menu();
    while (1) {
        u_readline(buf, sizeof(buf));
        u_print("\r\n");
        switch (buf[0]) {
            case '1': handle_occupancy();    break;
            case '2': handle_access_log();   break;
            case '3': handle_room_status();  break;
            case '4': handle_time_schedule();break;
            case '5': handle_sd_status();    break;
            case '6': handle_admin_entry();  break;
            case '0': break;
            default:  u_println("Invalid choice."); break;
        }
        show_main_menu();
    }
}

/* ── public init ─────────────────────────────────────────────────────── */

void serial_menu_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(MENU_UART, &cfg);
    uart_driver_install(MENU_UART, BUF_SIZE * 2, 0, 0, NULL, 0);
    xTaskCreate(serial_menu_task, "serial_menu", 4096, NULL, 2, NULL);
    ESP_LOGI(TAG, "Serial menu started");
}
