#include "../include/bss.h"

/*
 * Inicialização da seção .bss.
 *
 * Em C/assembly compilado para um SO, quem zera o .bss é o
 * "crt0" (rotina de início). Aqui o "SO" somos nós: este é um dos
 * primeiros passos do kernel, porque sem zerar o .bss as variáveis
 * globais (pilhas de tarefas, fila do teclado, tabelas GDT/IDT/FS)
 * viriam com lixo de memória.
 *
 * Os símbolos __bss_start/__bss_end NÃO estão definidos em C:
 * eles são definidos pelo linker script (linker/kernel.ld) como
 * marcadores de início/fim da seção. No C apenas tomamos seus
 * endereços (&...) e percorremos a área zerando byte a byte.
 */
extern char __bss_start;
extern char __bss_end;

void init_bss(void) {
    char *bss_start = &__bss_start;
    char *bss_end = &__bss_end;

    while (bss_start < bss_end) {
        *bss_start++ = 0;
    }
}