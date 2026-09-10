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

/*
 * ============================================================
 * ENTRY POINT DO KERNEL C
 * ============================================================
 * O bootloader termina com "jmp 0x10000". É para cá que ele pula
 * (KERNEL_OFFSET == KERNEL_BASE do linker script).
 *
 * NÃO EXISTE main(): ao contrário de um programa normal, aqui
 * não há runtime. Esta função _start é quem faz tudo - inclusive
 * zerar o .bss (init_bss) - e por convenção nunca retorna
 * (o último passo chama scheduler_begin que "nunca volta").
 *
 * A ordem das chamadas é IMPORTANTE:
 *   - o timer (PIT) só pode ser ligado DEPOIS que o handler de
 *     IRQ0 existir na IDT (senão a primeira interrupção quebraria);
 *   - a TSS precisa existir antes de qualquer interrupção vinda
 *     do ring 3 (que precisa de uma pilha de kernel - o TSS.esp0).
 *
 * Texto didático completo: docs/04-gdt-idt-pic.md e docs/10-fluxo.md
 * ============================================================
 */

extern void irq0_timer_stub(void);   /* assembly: vetor 32 → escalonador */
extern void prog_terminal(void);     /* terminou: tarefa de ring 3 */

__attribute__((section(".text.boot"), used))
void _start(void) {
    /* (1) Zera o .bss: sem isso as variáveis globais (pilhas de
     *     tarefas, fila de teclado, tabelas...) viriam com lixo. */
    init_bss();
    vga_clear();

    /* Banner de inicialização (memória mapeada direto). */
    vga_puts(10, 2, "Kernel Bare-Metal x86_64", VGA_COLOR_GREEN);
    vga_puts(10, 4, "Sistema de arquivos + terminal (tarefa ring 3)", VGA_COLOR_WHITE);

    /* (2) Infraestrutura do processador. */
    gdt_init();         /* GDT: segmentos de kernel (anel 0) e usuário (anel 3) */
    tss_init();         /* TSS: onde fica a pilha do kernel (ss0/esp0) */
    idt_init();         /* IDT: vetor → handler para 32 exceções + 16 IRQs */
    pic_remap();        /* PIC: remapeia IRQs para vetores 32+, mascara periféricos */

    /* (3) Drivers. */
    keyboard_init();    /* registra o handler do IRQ1 (teclado) */
    syscall_init();     /* gate de int 0x80 com DPL 3 (ring 3 pode chamar) */

    /* (4) IRQ0 deixa de usar o handler genérico e passa a chamar
     *     o escalonador (a troca de contexto é feita 100x/s).
     *     O vetor 32 da IDT agora aponta para o stub em assembly
     *     idt_stubs.asm → timer_tick. */
    idt_set_gate(32, (uint32_t)irq0_timer_stub, 0x08, 0x8E);

    pit_init(100);      /* liga o PIT em 100 Hz → a partir daqui vem irq0 */

    /* (5) Sistema de arquivos em RAM (docs/ dir, leiame.txt, ...). */
    fs_init();

    /* (6) Cria as tarefas: o terminal (ring 3) e a tarefa ociosa. */
    task_init();
    task_create(prog_terminal, 1);  /* is_user = 1 → roda no anel 3 */
    scheduler_add_idle();           /* idle: só hlt quando não tem ninguém */

    /* (7) Posiciona o cursor do console para as saídas do terminal. */
    console_init();
    console_set_cursor(0, 12);

    /* (8) PRIMEIRA TROCA DE CONTEXTO: monta o false frame do terminal
     *     e faz iret → o kernel "morre" e o terminal assume (ring 3).
     *     A partir daqui o kernel só reage a interrupções/syscalls. */
    scheduler_begin();      /* primeiro iret já liga as interrupções */
}