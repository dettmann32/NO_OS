#ifndef VGA_H
#define VGA_H

/*
 * Tela de texto VGA: memória mapeada em 0xB8000 (modo texto 80x25).
 * Cada célula = 2 bytes: o caractere (byte baixo) + cor (byte alto).
 * VGA_BUFFER é volatile porque o adaptador atualiza de forma
 * assíncrona em relação ao kernel (e para ninguém "otimizar" os
 * acessos para fora).
 */

#define VGA_BUFFER ((volatile unsigned short *)0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* Cores de 4 bits (nibble baixo do atributo = texto) */
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_RED   4
#define VGA_COLOR_WHITE 15

void vga_clear(void);
void vga_putchar(int x, int y, char c, unsigned char color);
void vga_puts(int x, int y, const char *str, unsigned char color);

#endif