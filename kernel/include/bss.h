#ifndef BSS_H
#define BSS_H

/*
 * Zera a seção .bss (variáveis globais não inicializadas). Em
 * programas com SO quem faz isso é o "crt0"; no kernel é o primeiro
 * passo de _start (kernel/init/bss.c).
 */

void init_bss(void);

#endif