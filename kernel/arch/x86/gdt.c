#include <stdint.h>
#include "../../include/gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t flags_lim_high;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr gdtp;
static struct tss tss;

static uint8_t kernel_stack[8192] __attribute__((aligned(16)));
static uint8_t user_stack[4096] __attribute__((aligned(16)));

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
    gdt[idx].base_low = base & 0xFFFF;
    gdt[idx].base_mid = (base >> 16) & 0xFF;
    gdt[idx].base_high = (base >> 24) & 0xFF;
    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].flags_lim_high = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    gdt[idx].access = access;
}

void gdt_init(void) {
    gdt_set_entry(0, 0, 0, 0, 0);                  /* null */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);      /* kernel code  (ring 0) */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);      /* kernel data  (ring 0) */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);      /* user code    (ring 3) */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);      /* user data    (ring 3) */
    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x00); /* TSS */

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint32_t)&gdt;

    __asm__ volatile("lgdt %0" : : "m"(gdtp));

    /* Recarrega os segmentos de dados no kernel (CS permanece 0x08) */
    __asm__ volatile(
        "movw $0x10, %%ax;"
        "movw %%ax, %%ds;"
        "movw %%ax, %%es;"
        "movw %%ax, %%fs;"
        "movw %%ax, %%gs;"
        "movw %%ax, %%ss;"
        : : : "ax");
}

void tss_init(void) {
    tss.ss0 = KERNEL_DS;
    tss.esp0 = (uint32_t)kernel_stack + sizeof(kernel_stack);

    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x00);
    __asm__ volatile("ltr %%ax" : : "a"(TSS_SEL));
}

void tss_set_esp0(uint32_t esp0) {
    tss.esp0 = esp0;
}

__attribute__((noreturn))
void enter_usermode(void (*entry)(void)) {
    uint32_t user_stack_top = (uint32_t)user_stack + sizeof(user_stack);

    /* Monta o frame de iret: SS, ESP, EFLAGS, CS, EIP */
    __asm__ volatile(
        "movl %0, %%esp;"
        "pushl %1;"             /* SS  (user data)   */
        "pushl %0;"             /* ESP (user stack)  */
        "pushl $0x202;"         /* EFLAGS (IF=1)     */
        "pushl %2;"             /* CS  (user code)   */
        "pushl %3;"             /* EIP (entry)       */
        "iret;"
        :
        : "r"(user_stack_top), "r"(USER_DS), "r"(USER_CS), "r"(entry)
        : "memory");

    for (;;) {
    }
}