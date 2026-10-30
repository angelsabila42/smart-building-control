#ifndef MANAGEMENT_CONSOLE_H
#define MANAGEMENT_CONSOLE_H

/*
 * Line-oriented UART0 console that feeds the serial management menu.
 * Output also goes to the same UART so the Wokwi serial monitor shows both
 * firmware logs and menu responses.
 */

void management_console_start(void);

/* serial_menu write callback. */
void management_console_write(void *context, const char *text);

#endif /* MANAGEMENT_CONSOLE_H */
