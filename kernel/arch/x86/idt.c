#include <stdint.h>
#include "../../include/idt.h"
#include "../../include/pic.h"
#include "../../include/io.h"
#include "../../include/vga.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt_table[IDT_ENTRIES];
static struct idt_ptr idtp;

#define IDT_STUB_COUNT 48

extern void *isr_stub_table[];

static void (*irq_handlers[16])(void);

static const char *exception_names[32] = {
    "Divisao por zero",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Fora de faixa",
    "Opcode invalido",
    "Dispositivo indisponivel",
    "Double fault",
    "Coprocessador de segmento",
    "TSS invalida",
    "Segmento ausente",
    "Falha de pilha",
    "Protecao geral",
    "Page fault",
    "Reservado",
    "Erro x87 FPU",
    "Alinhamento",
    "Machine check",
    "SIMD FPU",
    "Virtualizacao",
    "Controle de protecao",
    "Reservado",
    "Reservado",
    "Reservado",
    "Reservado",
    "Reservado",
    "Reservado",
    "Reservado",
    "Reservado",
    "Reservado",
    "Reservado"
};

void irq_register_handler(uint8_t irq, void (*handler)(void)) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

void idt_init(void) {
    idtp.limit = sizeof(idt_table) - 1;
    idtp.base = (uint32_t)&idt_table;

    /* Zera todas as 256 entradas */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_table[i].base_low = 0;
        idt_table[i].base_high = 0;
        idt_table[i].selector = 0;
        idt_table[i].zero = 0;
        idt_table[i].flags = 0;
    }

    /* Instala os stubs das 32 exceções + 16 IRQs */
    for (int i = 0; i < IDT_STUB_COUNT; i++) {
        uint32_t base = (uint32_t)isr_stub_table[i];
        idt_table[i].base_low = base & 0xFFFF;
        idt_table[i].base_high = (base >> 16) & 0xFFFF;
        idt_table[i].selector = 0x08;
        idt_table[i].zero = 0;
        idt_table[i].flags = 0x8E;
    }

    __asm__ __volatile__("lidt %0" : : "m"(idtp));
}

void idt_set_gate(uint8_t vec, uint32_t base, uint16_t selector, uint8_t flags) {
    idt_table[vec].base_low = base & 0xFFFF;
    idt_table[vec].base_high = (base >> 16) & 0xFFFF;
    idt_table[vec].selector = selector;
    idt_table[vec].zero = 0;
    idt_table[vec].flags = flags;
}

static void print_hex(uint32_t value, int row) {
    char buf[11] = "0x--------";
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex[(value >> (28 - i * 4)) & 0xF];
    }
    vga_puts(0, row, buf, VGA_COLOR_WHITE);
}

static void panic_exception(registers_t *r) {
    vga_clear();
    vga_puts(0, 2, "EXCECAO NAO TRATADA", VGA_COLOR_RED);
    vga_puts(0, 4, "Nome:", VGA_COLOR_WHITE);
    if (r->int_no < 32) {
        vga_puts(6, 4, exception_names[r->int_no], VGA_COLOR_RED);
    }
    vga_puts(0, 6, "EIP:", VGA_COLOR_WHITE);
    print_hex(r->eip, 6);
    vga_puts(0, 8, "Err:", VGA_COLOR_WHITE);
    print_hex(r->err_code, 8);

    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

void isr_handler(registers_t *r) {
    if (r->int_no < 32) {
        panic_exception(r);
        return;
    }

    if (r->int_no >= 40) {
        pic_send_eoi(r->int_no - 32);
    } else {
        outb(PIC1_COMMAND, PIC_EOI);
    }

    uint8_t irq = r->int_no - 32;
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
}