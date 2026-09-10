#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
void console_set_cursor(int x, int y);
void console_putchar(char c);
void console_write(const char *str);

#endif