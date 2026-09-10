#include <stdint.h>
#include "../include/console.h"
#include "../include/vga.h"
#include "../include/io.h"

static int cursor_x = 0;
static int cursor_y = 0;

/* Atualiza o cursor de hardware do VGA (portas 0x3D4/0x3D5) */
static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Sobe o texto uma linha e move o cursor para a última */
static void scroll(void) {
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA_BUFFER[(row - 1) * VGA_WIDTH + col] = VGA_BUFFER[row * VGA_WIDTH + col];
        }
    }
    for (int col = 0; col < VGA_WIDTH; col++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (unsigned short)' ' | (VGA_COLOR_WHITE << 8);
    }
    cursor_y = VGA_HEIGHT - 1;
}

void console_init(void) {
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void console_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    update_cursor();
}

void console_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = (unsigned short)' ' | (VGA_COLOR_WHITE << 8);
    } else if (c == '\t') {
        do {
            cursor_x++;
        } while (cursor_x % 8 != 0);
    } else {
        vga_putchar(cursor_x, cursor_y, c, VGA_COLOR_WHITE);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VGA_HEIGHT) {
        scroll();
    }

    update_cursor();
}

void console_write(const char *str) {
    while (*str) {
        console_putchar(*str++);
    }
}