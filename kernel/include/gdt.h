#ifndef GDT_H
#define GDT_H

/*
 * Seletores de segmento (a CPU usa estes números com a GDT de
 * kernel/arch/x86/gdt.c):
 *   - 0x08/0x10 = kernel (anel 0)
 *   - 0x1B/0x23 = usuário (anel 3 - o terminal)
 *   - 0x28      = TSS (pilha do kernel das interrupções do user)
 *
 * Note: user code termina em B e user data em 3 — o bit de RPL
 * (Requested Privilege Level) fica ligado nesses seletores, que é
 * o que leva a CPU a trocar para o anel 3 quando o iret os usa.
 */

#include <stdint.h>

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS   0x1B
#define USER_DS   0x23
#define TSS_SEL   0x28

void gdt_init(void);
void tss_init(void);
void tss_set_esp0(uint32_t esp0);
void enter_usermode(void (*entry)(void));

#endif