# 04 — Modo protegido: GDT, anéis de privilégio, IDT, TSS e PIC

> Arquivos: `kernel/arch/x86/gdt.c`, `kernel/arch/x86/idt.c`,
> `kernel/arch/x86/pic.c`, `kernel/arch/x86/idt_stubs.asm`

O kernel chegou em `_start` (C), mas o processador ainda está quase "cru".
Antes de qualquer coisa útil, o kernel precisa configurar as tabelas que
dão estrutura ao CPU: GDT (segmentos/privilégio), IDT (interrupções) e o
PIC (hardware de interrupções). Este doc explica cada uma.

## 4.1 Visão geral do que `_start` faz na ordem

```c
// kernel/main.c (resumido)
_start:
  init_bss();      // zera a seção .bss (globais não inicializadas)
  vga_clear();     // limpa a tela
  banner...        // imprime os cabeçalhos da tela
  gdt_init();      // 1. tabela de segmentos (anel 0 e 3)
  tss_init();      // 2. estado de pilha do kernel (esp0)
  idt_init();      // 3. tabela de interrupções
  pic_remap();     // 4. re-mapeia os IRQs para vetores > 31
  keyboard_init(); // 5. registra handler de IRQ1
  syscall_init();  // 6. gate de int 0x80 (DPL 3)
  idt_set_gate(32, irq0_timer_stub, ...); // 7. timer vai direto p/ escalonador
  pit_init(100);   // 8. dispara o timer (a partir daqui: multitarefa!)
  fs_init();       // 9. cria a árvore de arquivos em RAM
  task_init(); task_create(...); scheduler_add_idle(); // 10. tarefas
  scheduler_begin();  // 11. primeira troca de contexto (nunca volta)
```

Ordem importa: por ex., o timer só deve ser ligado **depois** que o handler
de IRQ0 já exista, senão a primeira interrupção quebraria tudo.

## 4.2 GDT — definindo segmentos e anéis

### Por que segmentos?

No modo protegido, todo acesso de memória passa por uma **verificação de
segmento**. A definição de cada segmento mora na GDT, e a CPU aplica:
privilégio, limites, permissões de leitura/escrita/execução.

### A entrada da GDT (8 bytes)

```c
struct gdt_entry {
    uint16_t limit_low;     // limite (bits 0–15)
    uint16_t base_low;      // base  (bits 0–15)
    uint8_t  base_mid;      // base  (bits 16–23)
    uint8_t  access;        // privilégio + tipo        ← o essencial
    uint8_t  flags_lim_high;// granularidade + limite (bits 16–19)
    uint8_t  base_high;     // base  (bits 24–31)
} __attribute__((packed));  // SEM padding: precisa ser exatamente 8 bytes
```

O campo `access` decide o anel e o tipo (bit 5 = "descritor de código").
Dois exemplos usados no projeto:

```c
access 0x9A  = 1001 1010b  → presente, anel 0 (bits 6-5=00), executável
access 0xFA  = 1111 1010b  → presente, anel 3 (bits 6-5=11), executável
access 0x92/0xF2          → idem, mas dados (bit 3 = 0)
```

### Seletores de segmento

Um seletor é o **índice** da GDT multiplicado por 8, mais o RPL
(nível de anel):

```
0x08 = entrada 1, RPL 0  (kernel code)    → kernel/main.c e handlers
0x10 = entrada 2, RPL 0  (kernel data)
0x1B = entrada 3, RPL 3  (user code   → terminal)
0x23 = entrada 4, RPL 3  (user data)
0x28 = entrada 5         (TSS — ver abaixo)
```

Estão definidos em `kernel/include/gdt.h` como constantes e usados em todo o
projeto (`0x08`, `0x10`, ... aparecem em `idt.c`, `idt_stubs.asm` etc.).

> Cuidado: num SO de verdade, segmentos quase não se usam mais (a memória
> fica na "paginada"). Aqui cada segmento tem base 0 e limite 4 GiB e são os
> RPLs que criam o isolamento ring 0/ring 3 (a paginação, que daria proteção
> de verdade, está além de escopo).

### TSS — a pilha do kernel para quando chega interrupção no ring 3

Quando o timer dispara com o terminal rodando (anel 3), a CPU precisa de uma
**pilha do anel 0** para empilhar o contexto. Ela não inventa: ela lê o
**TSS** (Task State Segment) → lendo os campos `esp0`/`ss0`. Por isso:

```c
tss.ss0  = KERNEL_DS;                         // segmento da pilha
tss.esp0 = kernel_stack + sizeof(kernel_stack); // topo da pilha do kernel
```

Cada tarefa também tem sua **própria** pilha de kernel; quando o escalonador
troca de tarefa, ele atualiza `tss.esp0` para o topo da pilha da tarefa que
passa a correr (isso é o que `tss_set_esp0()` faz — chamado em
`timer_tick` e `scheduler_begin`).

A instrução `ltr` ("load task register") marca na CPU qual TSS está ativa.

## 4.3 IDT — a tabela de interrupções

### O que é uma interrupção, de novo

Um sinal (do timer, do teclado, de uma divisão por zero, de um `int 0x80`)
chega à CPU, que:

1. Salva EFLAGS, CS, EIP (e, se veio do ring 3, também SS e ESP);
2. **Consulta a IDT no vetor correspondente**; se houve mudança de anel
   (3→0), usa `esp0` do TSS como pilha;
3. Pula para o handler daquele vetor.

### Entrada da IDT

```c
struct idt_entry {
    uint16_t base_low;      // endereço do handler (bits 0–15)
    uint16_t selector;      // CS do handler (geralmente 0x08)
    uint8_t  zero;          // sempre 0
    uint8_t  flags;         // tipo + DPL     ← o essencial
    uint16_t base_high;     // endereço (bits 16–31)
} __attribute__((packed));
```

`flags`:

- **0x8E** — interrupt gate: anel 0, desliga IF na entrada (evita
  re-entrância por interrupção enquanto o handler corre);
- **0xEE** — igual, mas **DPL 3**: permite o ring 3 disparar `int 0x80`.

O vetor 32 recebe `0x8E` e o vetor 0x80 recebe `0xEE` (syscall). O resto
recebe 0x8E por padrão (loop `IDT_STUB_COUNT`).

### Os stubs em assembly (idt_stubs.asm)

A IDT aponta para **stubs em assembly**, não para C. Por quê? Porque quando
a interrupção chega, o que está na pilha é exatamente o "lixo" que a CPU
empilhou (EIP, CS, EFLAGS...) sem ordem convenient, e precisamos converter
isso numa struct `registers_t` que o C usa.

Veja o stub genérico:

```asm
isr_common_stub:
    pusha                 ; empilha EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX
    push esp              ; passa ponteiro para o frame = registers_t*
    call isr_handler      ; C!
    add esp, 4            ; devolve o ponteiro
    popa                  ; restaura registradores
    add esp, 8            ; descarta int_no e err_code
    iret                  ; volta para o código interrompido
```

A pilha no momento do `push esp` precisa ter a ordem exata dos campos da
struct `registers_t` (`kernel/include/registers.h`):

```c
typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // ← o pusha
    uint32_t int_no;                                   // ← push do stub
    uint32_t err_code;                                 // ← push (0 se não há)
    uint32_t eip, cs, eflags;                          // ← empilhados pela CPU
} registers_t;
```

Isso é um **ABI da nossa responsabilidade**: a struct em C precisa espelhar
exatamente o que o assembly empilha. Se mudar um, quebra o outro.

> Detalhe de arquitetura: cada exceção tem seu **número** (vetor). O stub
> empilha esse número logo depois do `pusha` (`push dword %1`) e, para
> exceções que a CPU **não** empilha `err_code` por conta própria, o stub
> empilha um 0 no lugar (`ISR_NOERR`) para manter o layout fixo.

## 4.4 As exceções e o handler C (idt.c)

`isr_handler(registers_t *r)` recebe o frame de qualquer interrupção:

- `int_no < 32` → exceção da CPU → `panic_exception(r)` imprime na tela o
  que aconteceu (nome, EIP, código de erro) e trava.
- `int_no >= 32` → IRQ de hardware → envia **EOI** ao PIC (ver abaixo) e
  chama o handler registrado via `irq_handlers[irq]`.

O registrador `irq_register_handler(irq, handler)` é a "API" usada por
drivers: o teclado registra o handler de IRQ1, por exemplo.

## 4.5 PIC — o "gateway" físico das interrupções de hardware

O **8259 PIC** (Programmable Interrupt Controller) é o chip que liga os
IRQs dos periféricos (timer, teclado, mouse...) aos números de interrupção
da CPU. Como existem dois PICs, fala-se master e slave.

### O problema (e a solução: `pic_remap`)

Por padrão, os PICs mapeiam IRQ0–7 nos vetores **0–7**, que colidem com as
exceções da CPU (divisão por zero etc.). Então remapeamos com uma sequência
de comandos (ICW/write):

```c
outb(PIC1_COMMAND, 0x11);   // ICW1: iniciar, espere ICW4
outb(PIC1_DATA, PIC1_OFFSET); // ICW2: mapear IRQ0–7 → vetores 32–39
outb(PIC2_DATA, PIC2_OFFSET); //       IRQ8–15 → vetores 40–47
outb(PIC1_DATA, 0x04);      // ICW3: master sabe que tem slave na linha 2
outb(PIC2_DATA, 0x02);      // ICW3: slave se liga na linha 2 do master
outb(PIC1_DATA, 0x01);      // ICW4: modo 8086
```

Depois, uma máscara decide quais IRQs ficam ligados:

```c
outb(PIC1_DATA, 0xFC);   // 1111 1100: libera só IRQ0 (timer) e IRQ1 (teclado)
outb(PIC2_DATA, 0xFF);   // todos os do slave mascarados
```

Sem EOI, o PIC fica segurando a linha e **nunca mais** deixa chegar outra
interrupção. Por isso todo handler de hardware manda `pic_send_eoi(irq)`.

### Fluxo completo de um IRQ (ex.: IRQ0 = timer)

```
PIT gera pulso → PIC master identifica IRQ0 → CPU:
  1. salva EFLAGS/CS/EIP (e SS/ESP se veio do ring 3, usando TSS.esp0);
  2. consulta IDT[32] → PULA para irq0_timer_stub (assembly);
  3. o stub empilha err_code, int_no, pusha, e chama timer_tick (C);
  4. timer_tick escolhe a próxima tarefa, manda EOI, devolve o esp dela;
  5. o stub faz mov esp, eax; popa; add esp,8; iret → volta na nova tarefa.
```

Isso é a **preempção**: o timer "rouba" a CPU da tarefa atual 100x por
segundo e entrega à próxima. (Os detalhes do escalonador estão em
`docs/06-escalonador.md`.)

## 4.6 Dimensionamento das tabelas neste projeto

- **IDT**: 256 entradas (todas os vetores possíveis), mas o projeto
  preenche pelo menos as 48 primeiras (32 exceções + 16 IRQs).
- **GDT**: 6 entradas (null, kernel code/data, user code/data, TSS).

## Próximo passo

Com a infraestrutura pronta (GDT + IDT + PIC), os **drivers** podem
conversar com o hardware: `docs/05-drivers.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `kernel/arch/x86/gdt.c` | GDT, TSS, entrada no ring 3 (`enter_usermode`) |
| `kernel/arch/x86/idt.c` | IDT, exceções, `irq_register_handler` |
| `kernel/arch/x86/idt_stubs.asm` | stubs pusha→C→popa→iret |
| `kernel/arch/x86/pic.c` | re-mapeamento e EOI |
| `kernel/include/registers.h` | ABI da pilha ↔ struct |
| `kernel/include/gdt.h` | seletores e callbacks |