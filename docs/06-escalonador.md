# 06 — Multitarefa e o escalonador

> Arquivos: `kernel/scheduler/scheduler.c`, `kernel/arch/x86/idt_stubs.asm`

Este é o coração do sistema: a capacidade de rodar **várias tarefas
"ao mesmo tempo"** num único CPU intercalando-as rapidamente (round-robin).
Entender isso é entender 90% do que é um SO.

## 6.1 O modelo que este kernel usa

- Cada tarefa é um **TCB** (`task_t` em `kernel/include/task.h`) com
  endereços de pilhas, `pid`, estado e um campo mágico: `saved_esp`.
- Existem **duas pilhas por tarefa**:
  - `kstacks[t][8192]` → pilha **do kernel** (onde o contexto é salvo);
  - `ustacks[t][4096]` → pilha **do usuário** (o código do terminal usa).
- O `tss.esp0` aponta para o topo da pilha de kernel da tarefa **corrente**,
  para que o CPU saiba por onde começar quando vier uma interrupção do anel 3.
- O "rodízio" acontece no IRQ0: em cada tick, o `timer_tick()` escolhe a
  próxima tarefa READY.

## 6.2 Como uma tarefa nasce: montando um "falso contexto" (build_task_context)

A ideia é a mais elegante do projeto: uma tarefa nova é criada **como se já
estivesse sido preemptada**. Ou seja, escrevemos na pilha de kernel dela o
mesmo formato que a CPU deixaria ali se uma interrupção tivesse acontecido.

```c
static uint32_t build_task_context(uint32_t entry, int is_user,
                                   uint32_t kstack_top, uint32_t user_stack_top) {
    uint32_t *p = (uint32_t *)kstack_top;   // começa do topo

    if (is_user) {
        *--p = USER_DS;        // SS (o iret vai restaurar)
        *--p = user_stack_top; // ESP
        *--p = 0x202;          // EFLAGS (bit 9 IF=1 → interrupções ligadas)
        *--p = USER_CS;        // CS
        *--p = entry;          // EIP (por onde a tarefa começa)
        *--p = 0;              // err_code
        *--p = 0x20;           // int_no (padrão: como se fosse IRQ0)
    }

    // depois o "pusha" que o stub faria:
    *--p = 0;                  // eax
    *--p = 0;                  // ecx
    *--p = 0;                  // edx
    *--p = 0;                  // ebx
    *--p = prev_esp;           // esp  (o valor real no momento do pusha)
    *--p = 0;                  // ebp
    *--p = 0;                  // esi
    *--p = 0;                  // edi

    return (uint32_t)p;        // ← este é o saved_esp!
}
```

O formato resultante na pilha é **idêntico** ao da `registers_t`. Quando a
tarefa for escolhida, basta `mov esp, saved_esp; popa; add esp,8; iret` e ela
"acorda" no `entry`, como se tivesse sido interrompida.

## 6.3 A primeira troca: `scheduler_begin` → `task_frame_enter`

```c
void scheduler_begin(void) {
    current = ready_head;                     // pega a primeira tarefa
    tss_set_esp0(current->kstack_top);        // ensina o CPU onde está a pilha
    task_frame_enter(current->saved_esp);     // assembly: nunca retorna
}
```

O `task_frame_enter` no assembly é a versão "enxuta" da troca (mesmo
formato de pilha, sem chamada a C):

```asm
task_frame_enter:
    mov esp, [esp + 4]   ; carrega a pilha da tarefa (1º arg ainda no topo)
    popa
    add esp, 8           ; descarta int_no e err_code
    iret                 ; boom: agora estamos na tarefa (user ou idle)
```

A partir daqui as interrupções já estão ligadas (EFLAGS=0x202 tem IF=1) e o
timer passa a chamar `timer_tick` regularmente.

## 6.4 A troca de contexto no IRQ0: `irq0_timer_stub` → `timer_tick`

### O stub em assembly (idt_stubs.asm)

```asm
irq0_timer_stub:
    push dword 0            ; err_code (fictício)
    push dword 32           ; int_no
    jmp irq0_timer_common

irq0_timer_common:
    pusha                   ; salva EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX
    push esp                ; ponteiro para o registers_t atual
    call timer_tick         ; C: escolhe a próxima; EAX = esp do frame dela
    add esp, 4              ; remove ponteiro
    mov esp, eax            ; ★ TROCA DE CONTEXTO: troca de pilha!
    popa                    ; restaura os registradores da nova
    add esp, 8              ; descarta int_no/err_code
    iret                    ; volta para a tarefa que passou a rodar
```

O segredo: **salvar o contexto = gravar pilha; trocar contexto = trocar
`esp`**. O `timer_tick` recebe o `esp` do frame atual (onde o `pusha`
terminou), guarda em `current->saved_esp`, escolhe a próxima e **retorna o
`saved_esp` delas**. O `mov esp, eax` faz todo o resto: o `popa` + `iret`
"acorda" a tarefa seguinte exatamente no ponto em que ela foi interrompida.

### O C (scheduler.c)

```c
uint32_t timer_tick(uint32_t esp) {
    if (current && current->state != TASK_DONE) {
        current->saved_esp = esp;   // 1. guarda o contexto que acabou
        current->state = TASK_READY; // 2. devolve para a fila
    }

    pic_send_eoi(0);                // 3. libera o IRQ0 (não repetir)
    pit_tick_bump();                // 4. incrementa o contador

    task_t *next = pick_next();     // 5. round-robin: próxima READY
    if (next != NULL && next != current) {
        next->state = TASK_RUNNING;
        current = next;
        tss_set_esp0(current->kstack_top); // 6. esp0 do TSS = pilha nova
        return current->saved_esp;          // 7. "troque para esta pilha"
    }
    if (current != NULL) current->state = TASK_RUNNING;
    return esp;                              // 8. nada mudou, fica na mesma
}
```

`pick_next()` anda pela lista **circular** (`next`) e pega a próxima tarefa
com `TASK_READY`:

```c
static task_t *pick_next(void) {
    task_t *t = current;
    do {
        t = t->next;
        if (t->state == TASK_READY) return t;
    } while (t != current);
    return NULL;
}
```

## 6.5 Por que o IRQ0 foge do `isr_handler` e vai direto ao stub?

Muitas interrupções passam pelo `isr_handler` (que dá EOI e chama
`irq_handlers[irq]`). O IRQ0 **não**: em `kernel/main.c` a entrada 32 da IDT
é sobrescrita para apontar **diretamente** para `irq0_timer_stub`. O motivo:

- A troca de contexto precisa **alterar `esp`** e manipular `popa`/`iret` do
  chamador. Isso é impossível de se fazer de dentro de uma função C comum
  (o C não tem acesso ao registrador de pilha do chamado).
- Então o assembly faz o trabalho; o C apenas decide *qual* ponteiro de pilha
  retornar.

## 6.6 A tarefa ociosa (idle)

```c
static void idle_task(void) {
    for (;;) { __asm__ __volatile__("hlt"); }
}
```

Se não houver tarefa READY (ex.: o terminal chamou `exit`), o round-robin
caí no idle: ele dorme com `hlt` (paralisa a CPU até a próxima interrupção)
e devolve o controle. É ele que impede a CPU de girar a 100% sem fazer nada.

## 6.7 Estado e ciclo de vida

```
TASK_UNUSED → TASK_READY (task_create) → TASK_RUNNING (escolhida)
                                    │
                (timer_tick troca)  └→ TASK_READY ... (infinito)
                 SYS_EXIT   → TASK_DONE → sai da fila
```

## Para mentalizar

```
IRQ0 (tick 100Hz)
   │
   ▼  CPU salva contexto de TAREFA-ATUAL na pilha kernel dela
timer_tick(): current->saved_esp = ptr-atual
              pick_next() → TAREFA-PRÓXIMA
              return próxima->saved_esp
   ▼  stub: mov esp, eax; popa; iret
TAREFA-PRÓXIMA "acorda" exatamente onde foi pausada
```

## Próximo passo

A tarefa de usuário precisa de uma porta para o kernel: as **syscalls** —
`docs/07-syscalls.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `kernel/scheduler/scheduler.c` | TCBs, fila, `build_task_context`, `timer_tick` |
| `kernel/arch/x86/idt_stubs.asm` | `irq0_timer_stub`, `task_frame_enter` |
| `kernel/arch/x86/gdt.c` | `tss_set_esp0`, `enter_usermode` |
| `kernel/include/task.h` | estrutura `task_t` |
| `kernel/main.c` | `scheduler_begin` (primeira troca) |