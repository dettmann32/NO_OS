#include <stdint.h>
#include <stddef.h>
#include "../include/task.h"
#include "../include/gdt.h"
#include "../include/pic.h"
#include "../include/pit.h"

/*
 * ============================================================
 * ESCALONADOR (multitarefa preemptiva, round-robin)
 * ============================================================
 * O coração do sistema: vários processos "ao mesmo tempo" num só
 * CPU. A cada tick do PIT (IRQ0, 100x/s) o timer_tick() escolhe a
 * próxima tarefa READY e devolve o ponteiro de pilha dela; o stub
 * de IRQ0 (idt_stubs.asm) faz "mov esp, eax; popa; iret" e a
 * tarefa seguinte "acorda" exatamente onde tinha parado.
 *
 * Conceitos-chave:
 *   - CADA tarefa tem a própria pilha de kernel e de usuário;
 *   - o TSS.esp0 sempre aponta para o topo da pilha do kernel da
 *     tarefa CORRENTE (senão interrupções vindas do ring 3
 *     empilhariam no lugar errado);
 *   - uma tarefa nova nasce "como se já tivesse sido interrompida"
 *     (build_task_context monta o frame igual ao de um iret).
 *
 * Texto didático completo: docs/06-escalonador.md
 * ============================================================
 */

/* Tabela de tarefas */
static task_t tasks[MAX_TASKS];
static task_t *current = NULL;
static task_t *ready_head = NULL;
static uint32_t next_pid = 1;

/* Pilhas: cada tarefa tem a própria pilha de kernel (usada nas interrupções)
 * e, se for tarefa de usuário, a própria pilha de user. */
static uint8_t kstacks[MAX_TASKS][KERNEL_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t ustacks[MAX_TASKS][USER_STACK_SIZE] __attribute__((aligned(16)));

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
    }
    current = NULL;
    ready_head = NULL;
}

/* ---------------------------------------------------------------
 * Monta a pilha do kernel de uma tarefa nova como se ela tivesse
 * acabado de ser interrompida por um tick:
 *
 *   [pusha] [int_no/err] [iret frame: SS ESP EFLAGS CS EIP]
 *
 * Depois basta o escalonador fazer "mov esp, saved_esp; popa;
 * add esp, 8; iret" para "acordar" a tarefa no seu entry.
 *
 * Regra de ouro: esse layout é o ABI entre o C e o IRQ0 stub —
 * a struct registers_t (kernel/include/registers.h) e o
 * task_frame_enter (idt_stubs.asm) esperam exatamente isto.
 * ---------------------------------------------------------------
 */
static uint32_t build_task_context(uint32_t entry, int is_user,
                                   uint32_t kstack_top, uint32_t user_stack_top) {
    uint32_t *p = (uint32_t *)kstack_top;

    /* Frame do iret (empilhado de cima para baixo): */
    if (is_user) {
        *--p = USER_DS;               /* SS  do usuário        */
        *--p = user_stack_top;        /* ESP do usuário        */
        *--p = 0x202;                 /* EFLAGS (IF=1)         */
        *--p = USER_CS;               /* CS  do usuário        */
        *--p = entry;                 /* EIP (entrada)         */
        *--p = 0;                     /* err_code              */
        *--p = 0x20;                  /* int_no (vetor 32)     */
    } else {
        /* Tarefa de kernel (idle): sem SS/ESP na pilha */
        *--p = 0x202;                 /* EFLAGS (IF=1)         */
        *--p = 0x08;                  /* CS kernel             */
        *--p = entry;                 /* EIP (entrada)         */
        *--p = 0;                     /* err_code              */
        *--p = 0x20;                  /* int_no (vetor 32)     */
    }

    uint32_t prev_esp = (uint32_t)p;  /* esp antes do pusha     */

    /* O bloco do pusha, na ordem exata de pusha (EDI..EAX): */
    *--p = 0;                         /* eax                   */
    *--p = 0;                         /* ecx                   */
    *--p = 0;                         /* edx                   */
    *--p = 0;                         /* ebx                   */
    *--p = prev_esp;                  /* esp (slot do pusha)   */
    *--p = 0;                         /* ebp                   */
    *--p = 0;                         /* esi                   */
    *--p = 0;                         /* edi                   */

    return (uint32_t)p;               /* saved_esp (fim do pusha) */
}

int task_create(void (*entry)(void), int is_user) {
    /* Acha um slot livre na tabela de TCBs. */
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return -1;
    }

    task_t *t = &tasks[slot];
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->cwd = 0;                    /* diretório raiz do FS */

    uint32_t ktop = (uint32_t)(kstacks[slot] + KERNEL_STACK_SIZE);
    uint32_t utop = (uint32_t)(ustacks[slot] + USER_STACK_SIZE);
    t->kstack_top = ktop;
    t->user_stack_top = utop;
    t->saved_esp = build_task_context((uint32_t)entry, is_user, ktop, utop);

    /* Insere no fim da lista circular */
    if (ready_head == NULL) {
        ready_head = t;
        t->next = t;
    } else {
        task_t *last = ready_head;
        while (last->next != ready_head) {
            last = last->next;
        }
        last->next = t;
        t->next = ready_head;
    }

    return t->pid;
}

/* Tarefa ociosa (kernel, ring 0): só dorme. Garante CPU quando
 * nenhuma tarefa de usuário pode rodar. */
__attribute__((section(".text"), used))
static void idle_task(void) {
    for (;;) {
        __asm__ __volatile__("hlt");   /* dorme até a próxima interrupção */
    }
}

/* Próxima tarefa READY em round-robin a partir da atual */
static task_t *pick_next(void) {
    task_t *t = current;
    do {
        t = t->next;
        if (t->state == TASK_READY) {
            return t;
        }
    } while (t != current);
    return NULL;
}

/* Datada bem no início: entra na primeira tarefa. Nunca retorna. */
void scheduler_begin(void) {
    current = ready_head;
    if (current == NULL) {
        return;
    }
    current->state = TASK_RUNNING;

    /* publica o topo da pilha na TSS para o TSS.esp0 correto */
    extern void tss_set_esp0(uint32_t esp0);
    tss_set_esp0(current->kstack_top);

    task_frame_enter(current->saved_esp);   /* nunca retorna */
}

/* Marca a tarefa atual como DONE (syscall SYS_EXIT). Ela sai da
 * fila e o escalonador para de escalá-la. */
void scheduler_exit_current(void) {
    if (current) {
        current->state = TASK_DONE;
    }
}

task_t *task_current(void) {
    return current;
}

/* ---------------------------------------------------------------
 * Chamado a cada tick do PIT pela interrupção de hardware (IRQ0).
 * Recebe o esp do frame que o stub acabou de pushar (= registers_t
 * da tarefa que estava rodando) e devolve o esp do frame da próxima.
 *
 * O stub faz então: mov esp, eax; popa; add esp, 8; iret.
 * ---------------------------------------------------------------
 */
uint32_t timer_tick(uint32_t esp) {
    /* 1. Guarda o contexto da tarefa que estava rodando. */
    if (current && current->state != TASK_DONE) {
        current->saved_esp = esp;
        current->state = TASK_READY;
    }

    /* 2. Libera o IRQ0 (sem EOI o interrupt fica preso) e conta o tick. */
    pic_send_eoi(0);
    pit_tick_bump();

    /* 3. Escolhe a próxima (round-robin). */
    task_t *next = pick_next();
    if (next != NULL && next != current) {
        next->state = TASK_RUNNING;
        current = next;
        /* Atualiza a pilha do kernel da tarefa corrente no TSS:
         * a próxima interrupção/syscall vinda do ring 3 usará aqui. */
        extern void tss_set_esp0(uint32_t esp0);
        tss_set_esp0(current->kstack_top);
        return current->saved_esp;
    }

    /* 4. Ninguém para trocar (ou trocaríamos para nós mesmos): fica. */
    if (current != NULL) {
        current->state = TASK_RUNNING;
    }
    return esp;
}

/* Garante que a tarefa ociosa exista sempre na fila */
void scheduler_add_idle(void) {
    task_create((void (*)(void))idle_task, 0);
}