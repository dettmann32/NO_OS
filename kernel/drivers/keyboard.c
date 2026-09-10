#include <stdint.h>
#include "../include/keyboard.h"
#include "../include/io.h"
#include "../include/idt.h"

/*
 * ============================================================
 * TECLADO PS/2 (controlador 8042, scancode set 1)
 * ============================================================
 * O teclado não envia letras: envia SCANCODES (números) a cada
 * tecla apertada e a cada tecla solta. Este driver:
 *
 *   1. roda na interrupção IRQ1 (registrada em keyboard_init);
 *   2. lê o byte da porta 0x60;
 *   3. descobre se é tecla apertada ou solta (bit 7);
 *   4. traduz o scancode para ASCII usando duas tabelas
 *      (sem Shift e com Shift);
 *   5. põe o caractere numa FILA CIRCULAR para o sys_read
 *      (o terminal consome depois, em modo texto).
 *
 * Texto didático completo: docs/05-drivers.md
 * ============================================================
 */

#define KEYBOARD_DATA 0x60
#define KEYBUF_SIZE   128

/* Scancodes especiais (set 1) */
#define SC_ESC       0x01
#define SC_BACKSPACE 0x0E
#define SC_ENTER     0x1C
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_CAPS      0x3A

/* Apertar + soltar = bit 7 setado (ex.: apertar 'A'=0x1E, soltar=0x9E) */
#define KEY_RELEASED 0x80
#define SCANCODE_MAX 0x58

/* Tabela do scancode set 1 (layout US) sem Shift */
static const char scancode_low[SCANCODE_MAX] = {
     0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\','z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0
};

/* Tabela com Shift pressionado (símbolos e maiúsculas) */
static const char scancode_shift[SCANCODE_MAX] = {
     0, 0x1B, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0
};

static int shift_pressed = 0;
static int caps_lock = 0;

/* Fila de teclas para o sys_read (produtor/consumidor circular).
 * produtor = o IRQ1 (keyboard_handler); consumidor = keyboard_read(). */
static volatile char keybuf[KEYBUF_SIZE];
static volatile int keybuf_head = 0;
static volatile int keybuf_tail = 0;

/* Insere na fila; se estiver cheia, descarta (não tem para onde). */
static void keybuf_push(char c) {
    int next = (keybuf_head + 1) % KEYBUF_SIZE;
    if (next == keybuf_tail) {
        return;
    }
    keybuf[keybuf_head] = c;
    keybuf_head = next;
}

/* Consumidor: devolve o próximo caractere ou -1 se a fila estiver vazia. */
int keyboard_read(void) {
    if (keybuf_tail == keybuf_head) {
        return -1;
    }
    char c = keybuf[keybuf_tail];
    keybuf_tail = (keybuf_tail + 1) % KEYBUF_SIZE;
    return c;
}

/* Handler chamado no IRQ1 (por irq_register_handler). Roda no
 * contexto de interrupção: só mexe na fila, nada demorado. */
static void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA);

    if (scancode & KEY_RELEASED) {              /* tecla solta */
        scancode &= ~KEY_RELEASED;
        if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
            shift_pressed = 0;
        }
        return;
    }

    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_pressed = 1;
        return;
    }
    if (scancode == SC_CAPS) {
        caps_lock = !caps_lock;
        return;
    }

    if (scancode >= SCANCODE_MAX) {
        return;
    }

    char c = shift_pressed ? scancode_shift[scancode] : scancode_low[scancode];
    if (c == 0) {
        return;
    }

    /* Caps Lock inverte somente letras (Shift+Caps desliga a maiúscula) */
    if (c >= 'a' && c <= 'z' && caps_lock) {
        c -= 32;
    } else if (c >= 'A' && c <= 'Z' && caps_lock) {
        c += 32;
    }

    keybuf_push(c);
}

void keyboard_init(void) {
    irq_register_handler(1, keyboard_handler);
}