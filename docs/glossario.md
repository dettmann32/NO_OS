# Glossário

Termos usados no código e na documentação, explicados de forma simples.

## A

- **Anel (ring)**: nível de privilégio da CPU x86 em modo protegido. Ring 0
  = máximo (kernel), ring 3 = mínimo (usuário). (`docs/01-conceitos.md`)
- **Anel 0 / anel 3**: ver "Anel".

## B

- **Bare-metal**: programar o hardware diretamente, sem sistema operacional
  por baixo. Este projeto é um exemplo.

## C

- **CPL (Current Privilege Level)**: o anel do código que está executando
  agora (bits 0-1 de CS). É o que define as permissões no momento.
- **CR0**: registrador de controle da CPU; o bit 0 (bit PE) é o modo
  protegido.

## D

- **DAP (Disk Address Packet)**: estrutura de 16 bytes passada ao BIOS na
  leitura estendida LBA (`boot/boot.asm`).
- **DPL (Descriptor Privilege Level)**: o anel "mínimo" para usar aquele
  descritor/gate. Ex.: gate da IDT com DPL 3 permite `int 0x80` do ring 3.
- **Driver**: código que sabe conversar com um dispositivo.
- **DS/ES/FS/GS/SS**: registradores de segmento de *dados* e *pilha*.
  Apontam para entradas da GDT.

## E

- **EOI (End Of Interrupt)**: aviso que se manda ao PIC (`0x20`) dizendo que
  o handler terminou; sem isso o IRQ fica preso.
- **EFLAGS**: registrador de flags; o bit 9 (IF) habilita interrupções.
- **Exceção**: interrupção **do próprio processador** (divisão por zero,
  page fault...). Vetores 0-31.

## F

- **fd (file descriptor)**: índice de arquivo aberto (como no Linux).
  Aqui: índices de `fds[]` em `kernel/syscall/syscall.c`.
- **Frame (de iret)**: a pilha que a CPU empilha numa interrupção
  (EFLAGS, CS, EIP...). Este projeto monta "frames falsos" para criar
  tarefas (`docs/06-escalonador.md`).

## G

- **GDT (Global Descriptor Table)**: tabela que define segmentos e anéis da
  CPU. A instrução `lgdt` a carrega.

## I

- **IDT (Interrupt Descriptor Table)**: tabela que mapeia vetor →
  handler. A instrução `lidt` a carrega.
- **IRQ (Interrupt Request)** : sinal de interrupção de hardware (timer,
  teclado...). O PIC os entrega à CPU como vetores.
- **ISA**/**portas**: registradores de hardware acessados por `in`/`out`.
- **iret**: instrução que *volta* de interrupção/syscall: restaura
  EFLAGS, CS, EIP (e SS/ESP se veio do anel 3). O nosso "retorno" do
  ring 0 para ring 3.

## L

- **LBA (Logical Block Addressing)** : numeração linear de setores do disco
  (setor 0, 1, 2...), usada nas leituras estendidas do BIOS.
- **ltr**: instrução que define a TSS ativa.

## M

- **Memória mapeada**: periférico "visto" como memória (ex.: tela em
  `0xB8000`).
- **Modo protegido**: modo 32 bits com GDT, anéis, limites e proteções.
- **Modo real**: modo 16 bits usado pelo bootloader/BIOS.

## P

- **PIC (Programmable Interrupt Controller)**: chip (8259) que faz a ponte
  IRQ → vetor. Tem master e slave.
- **PIT (Programmable Interval Timer)** : timer 8253/8254 que gerou os
  ticks do nosso escalonador (IRQ0 @ 100 Hz).
- **pusha/popa**: instruções que empilham/desempilham os 8 registradores
  gerais de uma vez. Usadas para salvar contexto.

## R

- **registers_t**: struct que representa o "estado da CPU" numa pilha de
  interrupção (`kernel/include/registers.h`).
- **RPL (Requested Privilege Level)** : anel "pedido" num seletor de
  segmento (nos 2 bits baixos).
- **Round-robin**: escalonamento cíclico (cada tarefa em rodízio).

## S

- **Scancode**: byte enviado pelo teclado (`0x1E` = 'A', bit 7 = tecla solta).
- **Seletor de segmento**: índice da GDT × 8 + solicitação de anel
  (ex.: `0x08` = kernel code, `0x1B` = user code).
- **Syscall**: chamada de sistema — pedido do ring 3 ao kernel via `int 0x80`.
- **SYS_xxx**: macros com os números das syscalls (`kernel/include/syscall.h`).

## T

- **TCB (Task Control Block)** : estrutura que descreve uma tarefa
  (`task_t`).
- **TSS (Task State Segment)** : área da GDT que guarda `esp0` (pilha do
  kernel) — usada quando uma interrupção chega no ring 3.
- **Tick**: uma batida do timer (10 ms a 100 Hz).

## V

- **Vetor (de interrupção)**: número de 0 a 255 que identifica um evento.
  Usado como índice na IDT.

## Recursos externos recomendados

- [OSDev Wiki](https://wiki.osdev.org/) — enciclopédia de sistemas
- [Source of the "Writing an OS in Rust" (Philip Oppermann)](https://os.phil-opp.com/) —
  explica exatamente as mesmas etapas, em ritmo didático
- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) —
  manual oficial da CPU (Volume 3: system programming)