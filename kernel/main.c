#include <stdint.h>
#include "include/bss.h"
#include "include/vga.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/pic.h"
#include "include/pit.h"
#include "include/console.h"
#include "include/keyboard.h"
#include "include/syscall.h"
#include "include/task.h"
#include "include/fs.h"

extern void irq0_timer_stub(void);

extern void prog_terminal(void);

__attribute__((section(".text.boot"), used))
void _start(void) {
    init_bss();
    vga_clear();

    vga_puts(10, 2, "Kernel Bare-Metal x86_64", VGA_COLOR_GREEN);
    vga_puts(10, 4, "Sistema de arquivos + terminal (tarefa ring 3)", VGA_COLOR_WHITE);

    gdt_init();
    tss_init();
    idt_init();
    pic_remap();
    keyboard_init();
    syscall_init();

    /* IRQ0 deixa de usar o handler genérico e passa a chamar o escalonador */
    idt_set_gate(32, (uint32_t)irq0_timer_stub, 0x08, 0x8E);

    pit_init(100);
    fs_init();

    task_init();
    task_create(prog_terminal, 1);
    scheduler_add_idle();

    console_init();
    console_set_cursor(0, 12);

    scheduler_begin();      /* primeiro iret já liga as interrupções */
}