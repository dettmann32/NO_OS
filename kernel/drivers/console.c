#include <stdint.h>
#include "../include/console.h"
#include "../include/vga.h"
#include "../include/io.h"

/*
 * ============================================================
 * CONSOLE — terminal com cursor de hardware e scroll
 * ============================================================
 * O VGA apenas escreve numa célula fixa. O console é a camada
 * que mantém A POSIÇÃO ATUAL do cursor (cursor_x, cursor_y) e
 * implementa os caracteres especiais:
 *
 *   '\n' → quebra de linha (cursor_x = 0, cursor_y++)
 *   '\b' → backspace (apaga o char anterior)
 *   '\t' → tab (alinhar a 8 colunas)
 *
 * Quando o cursor passa da última linha, faz SCROLL (o texto
 * sobe uma linha e a última fica em branco).
 *
 * Além disso controla o CURSOR DE HARDWARE do VGA: o pisca-pisca
 * da tela, configurado pelas portas CRTC 0x3D4/0x3D5.
 *
 * Texto didático completo: docs/05-drivers.md
 * ============================================================
 */

static int cursor_x = 0;
static int cursor_y = 0;

/*
 * Cursor de hardware (o "pisca" que o próprio adaptador desenha).
 * Posição = cursor_y * 80 + cursor_x (0..1999), em 2 bytes (low/high).
 * Escreve-se nas portas 0x3D4/0x3D5 (CRTC): primeiro escolhe o
 * registrador (0x0F = cursor low, 0x0E = cursor high), depois o valor.
 */
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

/* A "primitiva" do terminal: um caractere, com todas as regras. */
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

    /* Quebra de linha automática ao chegar na borda direita. */
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    /* Scroll automático ao passar do fim da tela. */
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