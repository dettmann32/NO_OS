#include "../include/vga.h"

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = (unsigned short)' ' | ((unsigned short)VGA_COLOR_WHITE << 8);
    }
}

void vga_putchar(int x, int y, char c, unsigned char color) {
    int index = y * VGA_WIDTH + x;
    VGA_BUFFER[index] = (unsigned short)c | ((unsigned short)color << 8);
}

void vga_puts(int x, int y, const char *str, unsigned char color) {
    int i = 0;
    while (str[i] != '\0') {
        vga_putchar(x + i, y, str[i], color);
        i++;
    }
}