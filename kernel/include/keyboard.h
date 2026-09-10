#ifndef KEYBOARD_H
#define KEYBOARD_H

/*
 * Teclado PS/2 (kernel/drivers/keyboard.c): o IRQ1 traduz
 * scancodes e enfileira caracteres ASCII; keyboard_read() é o
 * consumidor usado pela syscall SYS_READ.
 */

void keyboard_init(void);
int keyboard_read(void);

#endif