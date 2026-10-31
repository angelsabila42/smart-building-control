#include "management/management_console.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "management/serial_menu.h"

#define CONSOLE_TAG "MGMT_CONSOLE"
#define CONSOLE_UART UART_NUM_0
#define CONSOLE_BAUD_RATE 115200
#define CONSOLE_RX_BUFFER 2048
#define CONSOLE_LINE_MAX 160
#define CONSOLE_TASK_STACK 8192
#define CONSOLE_TASK_PRIORITY 2

static bool console_started = false;

void management_console_write(void *context, const char *text)
{
    (void)context;

    if (text == NULL) {
        return;
    }

    uart_write_bytes(CONSOLE_UART, text, strlen(text));
}

static void echo(const char *text)
{
    uart_write_bytes(CONSOLE_UART, text, strlen(text));
}

static void console_task(void *argument)
{
    (void)argument;

    serial_menu_print_banner();
    serial_menu_print_prompt();

    char line[CONSOLE_LINE_MAX];
    size_t length = 0;
    bool discarding = false;
    bool previous_was_carriage_return = false;

    while (true) {
        uint8_t byte = 0;
        const int received = uart_read_bytes(CONSOLE_UART, &byte, 1, pdMS_TO_TICKS(100));

        if (received <= 0) {
            continue;
        }

        const char value = (char)byte;

        if (value == '\r' || value == '\n') {
            /* Treat CRLF as one line ending. */
            if (value == '\n' && previous_was_carriage_return) {
                previous_was_carriage_return = false;
                continue;
            }

            previous_was_carriage_return = (value == '\r');

            echo("\r\n");

            if (discarding) {
                discarding = false;
                length = 0;
                serial_menu_print_prompt();
                continue;
            }

            line[length] = '\0';

            if (length > 0) {
                serial_menu_execute(line);
            }

            length = 0;
            serial_menu_print_prompt();
            continue;
        }

        previous_was_carriage_return = false;

        if (discarding) {
            continue;
        }

        if (value == '\b' || value == 0x7f) {
            if (length > 0) {
                length--;
                echo("\b \b");
            }
            continue;
        }

        if (value < 0x20 || value > 0x7e) {
            continue;
        }

        if (length >= CONSOLE_LINE_MAX - 1) {
            echo("\r\nERR: line too long\r\n");
            serial_menu_print_prompt();
            discarding = true;
            length = 0;
            continue;
        }

        line[length++] = value;
        uart_write_bytes(CONSOLE_UART, &value, 1);
    }
}

void management_console_start(void)
{
    if (console_started) {
        return;
    }

    const uart_config_t config = {
        .baud_rate = CONSOLE_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t result = uart_driver_install(CONSOLE_UART, CONSOLE_RX_BUFFER, 0, 0, NULL, 0);

    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(CONSOLE_TAG, "UART driver install failed: %s", esp_err_to_name(result));
        return;
    }

    result = uart_param_config(CONSOLE_UART, &config);

    if (result != ESP_OK) {
        ESP_LOGW(CONSOLE_TAG, "UART configuration failed: %s", esp_err_to_name(result));
    }

    uart_flush_input(CONSOLE_UART);

    if (xTaskCreate(console_task,
                    "mgmt_console",
                    CONSOLE_TASK_STACK,
                    NULL,
                    CONSOLE_TASK_PRIORITY,
                    NULL) != pdPASS) {
        ESP_LOGE(CONSOLE_TAG, "Could not start console task");
        return;
    }

    console_started = true;
    ESP_LOGI(CONSOLE_TAG, "Serial console started on UART0 (%d baud)", CONSOLE_BAUD_RATE);
}
