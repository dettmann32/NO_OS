#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no;
    uint32_t err_code;
    uint32_t eip, cs, eflags;
} registers_t;

#endif