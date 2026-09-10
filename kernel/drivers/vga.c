#include "../include/vga.h"

/*
 * ============================================================
 * VGA — tela de texto (modo texto 80x25)
 * ============================================================
 * A tela VGA em modo texto é "memória mapeada" no endereço
 * 0xB8000 (VGA_BUFFER). Cada célula ocupa 2 bytes:
 *
 *   byte par  = caractere ASCII
 *   byte ímpar = atributos de cor:
 *                nibble alto = cor de FUNDO (4 bits)
 *                nibble baixo = cor do TEXTO (4 bits)
 *
 * A tela tem 80 colunas x 25 linhas = 2000 células = 4000 bytes.
 *
 * Texto didático completo: docs/01-conceitos.md e docs/05-drivers.md
 * ============================================================
 */

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = (unsigned short)' ' | ((unsigned short)VGA_COLOR_WHITE << 8);
    }
}

void vga_putchar(int x, int y, char c, unsigned char color) {
    int index = y * VGA_WIDTH + x;      /* linha * 80 + coluna → nº da célula */
    VGA_BUFFER[index] = (unsigned short)c | ((unsigned short)color << 8);
}

void vga_puts(int x, int y, const char *str, unsigned char color) {
    int i = 0;
    while (str[i] != '\0') {
        vga_putchar(x + i, y, str[i], color);
        i++;
    }
}