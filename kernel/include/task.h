#ifndef TASK_H
#define TASK_H

/*
 * TCB (Task Control Block) + API do escalonador round-robin.
 * Cada tarefa tem a própria pilha de kernel e (se de user) de
 * usuário; o contexto salvo fica como um frame de iret em
 * saved_esp (veja scheduler.c e idt_stubs.asm).
 */

#include <stdint.h>

#define MAX_TASKS         8
#define KERNEL_STACK_SIZE 8192
#define USER_STACK_SIZE   4096

typedef enum {
    TASK_UNUSED,        /* slot livre */
    TASK_READY,         /* na fila, aguardando CPU */
    TASK_RUNNING,       /* executando */
    TASK_DONE           /* terminou (saiu da fila) */
} task_state_t;

typedef struct task {
    uint32_t saved_esp;         /* onde o contexto (frame do iret) está na pilha do kernel */
    uint32_t pid;
    task_state_t state;
    uint32_t cwd;               /* diretório atual processo (nó do FS) */
    uint32_t kstack_top;        /* topo da pilha do kernel (usado no TSS.esp0) */
    uint32_t user_stack_top;    /* topo da pilha do usuário (só p/ tarefas de user) */
    struct task *next;
} task_t;

void task_init(void);
int task_create(void (*entry)(void), int is_user);
void scheduler_add_idle(void);
void scheduler_begin(void);
void scheduler_exit_current(void);
uint32_t timer_tick(uint32_t esp);

task_t *task_current(void);

extern void task_frame_enter(uint32_t saved_esp);

#endif