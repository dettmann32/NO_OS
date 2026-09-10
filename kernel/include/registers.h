#ifndef REGISTERS_H
#define REGISTERS_H

/*
 * registers_t = o CONTEXTO de uma tarefa quando ela é interrompida.
 *
 * IMPORTANTE (contrato ABI): a ordem destes campos espelha EXATAMENTE
 * o que os stubs em kernel/arch/x86/idt_stubs.asm empilham:
 *
 *      pusha  → edi esi ebp esp ebx edx ecx eax  (8 dwords)
 *      stubs  → int_no, err_code                 (2 dwords)
 *      CPU    → eip, cs, eflags                  (3 dwords, p/ iret)
 *
 * Quem roda abaixo deste struct na pilha é o que o iret restaura.
 */

#include <stdint.h>

typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no;
    uint32_t err_code;
    uint32_t eip, cs, eflags;
} registers_t;

#endif