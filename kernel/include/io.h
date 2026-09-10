#ifndef IO_H
#define IO_H

/*
 * Primitivas de portas de I/O, inlinadas pelo compilador.
 * A CPU x86 separa "mundo de memória" e "mundo de portas" — para
 * falar com o PIC, PIT, teclado, etc. usa-se estas instruções
 * (outb escreve na porta, inb lê, io_wait dá um "respiro").
 */

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif