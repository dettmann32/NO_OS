#ifndef IDT_H
#define IDT_H

/*
 * IDT (kernel/arch/x86/idt.c): mapeia vetores 0-255 para handlers.
 * Faixas usadas: 0-31 exceções, 32-47 IRQs (do remap do PIC),
 * 0x80 = syscalls. idt_set_gate serve para sobrescrever um vetor
 * (ex.: vetor 32 → escalonador) e para abrir o DPL 3 (0x80).
 */

#include <stdint.h>
#include "registers.h"

#define IDT_ENTRIES 256

void idt_init(void);
void idt_set_gate(uint8_t vec, uint32_t base, uint16_t selector, uint8_t flags);
void irq_register_handler(uint8_t irq, void (*handler)(void));

#endif