#include <stdint.h>
#include "../../include/idt.h"
#include "../../include/pic.h"
#include "../../include/io.h"
#include "../../include/vga.h"

/*
 * ============================================================
 * IDT (Interrupt Descriptor Table) + exceções + registro de IRQ
 * ============================================================
 * A IDT diz à CPU o que fazer para cada "evento":
 *   - exceções (vetores 0-31, erros da própria CPU);
 *   - IRQs de hardware (vetores 32-47, remapeados via PIC);
 *   - syscalls (vetor 0x80, int de software do ring 3).
 *
 * Quando um evento acontece, a CPU salva o contexto, consulta a
 * IDT no vetor correspondente e PULA para o handler (assembly).
 *
 * Texto didático completo: docs/04-gdt-idt-pic.md
 * ============================================================
 */

/* Entrada da IDT (8 bytes, sem padding):
 *   base/selector = endereço do handler + CS onde ele roda
 *   flags         = tipo de gate + nível de privilégio (DPL)
 *                   0x8E = interrupt gate, anel 0
 *                   0xEE = idem, mas DPL 3 (ring 3 pode disparar;
 *                          usado pelo int 0x80 das syscalls)
 */
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

/* Ponteiro usado pela instrução lidt. */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt_table[IDT_ENTRIES];   /* a tabela (256 vetores) */
static struct idt_ptr idtp;

/* Quantas entradas geramos a partir da tabela de stubs do assembly:
 * 32 exceções + 16 IRQs. */
#define IDT_STUB_COUNT 48

extern void *isr_stub_table[];   /* definida em idt_stubs.asm */

/* Handlers de driver registrados por irq_register_handler().
 * O teclado registra handler do IRQ1 aqui. O IRQ0 (timer) NÃO
 * usa este vetor: vai direto para o escalonador. */
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

/* API para drivers registrarem handlers de IRQ de hardware. */
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

    /* Instala os stubs das 32 exceções + 16 IRQs.
     * Cada entrada aponta para um rótulo assembly; selector 0x08
     * (kernel code, anel 0); flags 0x8E (interrupt gate). */
    for (int i = 0; i < IDT_STUB_COUNT; i++) {
        uint32_t base = (uint32_t)isr_stub_table[i];
        idt_table[i].base_low = base & 0xFFFF;
        idt_table[i].base_high = (base >> 16) & 0xFFFF;
        idt_table[i].selector = 0x08;
        idt_table[i].zero = 0;
        idt_table[i].flags = 0x8E;
    }

    /* lidt: diz à CPU onde está a IDT. Daqui em diante as
     * interrupções já têm handler. */
    __asm__ __volatile__("lidt %0" : : "m"(idtp));
}

/* Permite sobrescrever uma entrada específica da IDT depois do
 * idt_init (ex.: o vetor 32 para o escalonador, o 0x80 p/ syscall).
 * flags decidem o DPL (0xEE = ring 3 pode disparar). */
void idt_set_gate(uint8_t vec, uint32_t base, uint16_t selector, uint8_t flags) {
    idt_table[vec].base_low = base & 0xFFFF;
    idt_table[vec].base_high = (base >> 16) & 0xFFFF;
    idt_table[vec].selector = selector;
    idt_table[vec].zero = 0;
    idt_table[vec].flags = flags;
}

/* Mostra um número em hex na tela (para o panic). */
static void print_hex(uint32_t value, int row) {
    char buf[11] = "0x--------";
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex[(value >> (28 - i * 4)) & 0xF];
    }
    vga_puts(0, row, buf, VGA_COLOR_WHITE);
}

/* O que fazer quando um erro do CPU (exceção não tratada) acontece. */
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

/*
 * Handler central de interrupções (exceções + IRQs que NÃO são o
 * timer). Recebe o registers_t que o stub do assembly montou.
 */
void isr_handler(registers_t *r) {
    /* int_no < 32 → exceção da própria CPU: panico. */
    if (r->int_no < 32) {
        panic_exception(r);
        return;
    }

    /* EOI: avisa o PIC que a interrupção foi atendida.
     * slave (irq >= 8) precisa de EOI nos DOIS PICs. */
    if (r->int_no >= 40) {
        pic_send_eoi(r->int_no - 32);
    } else {
        outb(PIC1_COMMAND, PIC_EOI);
    }

    /* Chama o handler registrado pelo driver (ex.: teclado). */
    uint8_t irq = r->int_no - 32;
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq]();
    }
}