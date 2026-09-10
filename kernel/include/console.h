#ifndef CONSOLE_H
#define CONSOLE_H

/*
 * Console (kernel/drivers/console.c): camada sobre o VGA que mantém
 * cursor + scroll + caracteres especiais ('\n', '\b', '\t'). É o
 * "stdout" do kernel e para onde o SYS_WRITE do terminal escreve.
 */

void console_init(void);
void console_set_cursor(int x, int y);
void console_putchar(char c);
void console_write(const char *str);

#endif