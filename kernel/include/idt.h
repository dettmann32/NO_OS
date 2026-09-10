#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include "registers.h"

#define IDT_ENTRIES 256

void idt_init(void);
void idt_set_gate(uint8_t vec, uint32_t base, uint16_t selector, uint8_t flags);
void irq_register_handler(uint8_t irq, void (*handler)(void));

#endif