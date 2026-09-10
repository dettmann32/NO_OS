# Documentação Didática — Kernel Bare-Metal x86

> Para quem já sabe programar em **C**, mas nunca escreveu um sistema
> operacional. Aqui cada conceito é explicado do zero, no contexto real
> do código deste projeto.

## Como ler esta documentação

Os arquivos estão numerados em **ordem de leitura**. Cada um termina com
uma seção **"Código-fonte correspondente"** que liga a teoria aos arquivos
reais do projeto.

| # | Arquivo | O que explica |
|---|---------|-------------------------------|
| 1 | `docs/01-conceitos.md` | O que significa programar "bare-metal": CPU, memória, portas de I/O, BIOS, anéis de proteção |
| 2 | `docs/02-boot.md` | Bootloader: do reset do PC até carregar o kernel em memória |
| 3 | `docs/03-build.md` | Como o código vira imagem de disco (Makefile + linker script) |
| 4 | `docs/04-gdt-idt-pic.md` | Modo protegido: GDT, anéis 0/3, TSS, IDT e o PIC (interrupções de hardware) |
| 5 | `docs/05-drivers.md` | Periféricos: tela VGA, terminal com cursor, timer PIT e teclado PS/2 |
| 6 | `docs/06-escalonador.md` | Multitarefa: contextos, troca de tarefas e o coração da preempção |
| 7 | `docs/07-syscalls.md` | Entrando no ring 3 e chamando o kernel via `int 0x80` |
| 8 | `docs/08-filesystem.md` | Sistema de arquivos em RAM (árvore de diretórios em memória) |
| 9 | `docs/09-terminal.md` | O shell do usuário: leitura de linha, parser e comandos |
| 10 | `docs/10-fluxo.md` | Linha do tempo completa: do power-on até o prompt `/$` |
| 11 | `docs/glossario.md` | Glossário de termos (ISR, IRQ, EOI, DPL, CPL, scancode, frame...) |

## O projeto em 30 segundos

Este repositório implementa um **micro-kernel x86** que:

1. **Boota** sozinho num PC/QEMU (sem Linux, sem nada embaixo);
2. Carrega um **kernel C** (32 bits) do disco para a memória;
3. Configura **modo protegido** com dois níveis de privilégio (anel 0 = kernel, anel 3 = aplicação);
4. Instala **interrupções**: timer (PIT), teclado (PS/2) e exceções;
5. Implementa um **escalonador preemptivo** (troca de tarefa a cada tick do timer);
6. Abre um canal de **syscalls** (`int 0x80`) para o código de usuário;
7. Mantém um **sistema de arquivos em RAM**;
8. Roda um **shell (terminal)** como tarefa de ring 3.

## Como rodar

```bash
make            # compila bootloader + kernel + monta a imagem de disquete
make run        # abre no QEMU (ou: ./scripts/run_qemu.sh)
```

Dentro do terminal que aparece:

```
Kernel Bare-Metal x86_64 - Terminal
Digite 'help' para a lista de comandos.

/$ ls
docs/
usr/
inicio.txt
/$ cat inicio.txt
terminal pronto: ls, cd, mkdir, touch, echo, cat, rm
/$ cd docs
/docs$ ls
leiame.txt
projetos/
```

Comandos disponíveis: `ls`, `cd`, `mkdir`, `touch`, `echo >`, `cat`, `rm`, `pwd`, `clear`, `help`, `exit`.

## Mapa mental dos diretórios

```
SO/
├── boot/boot.asm             # Bootloader (assembly 16 bits → 32 bits)
├── kernel/
│   ├── main.c                # Ponto de entrada do kernel em C (_start)
│   ├── arch/x86/             # Coisas específicas do processador x86
│   │   ├── gdt.c             # Tabela de segmentos (anéis 0 e 3) + TSS
│   │   ├── idt.c             # Tabela de interrupções + exceções
│   │   ├── idt_stubs.asm     # Stubs em assembly (interrupção → C)
│   │   └── pic.c             # Controlador de interrupção programável
│   ├── drivers/              # Periféricos
│   │   ├── vga.c             #   textão na tela (modo texto 80x25)
│   │   ├── console.c         #   terminal com cursor, scroll, Enter/Backspace
│   │   ├── pit.c             #   timer de 100 Hz
│   │   └── keyboard.c        #   teclado PS/2 (scancode → caractere)
│   ├── init/bss.c            # Zera a seção de dados não inicializados
│   ├── scheduler/            # Escalonador (multitarefa preemptiva)
│   ├── syscall/              # Dispatcher de syscalls (int 0x80)
│   ├── fs/                   # Sistema de arquivos em RAM
│   └── include/              # Headers públicos do kernel
├── user/user.c               # O terminal/shell (roda em ring 3)
├── linker/kernel.ld          # Linker script (organização na memória)
└── scripts/                  # Scripts de teste/execução
```

## Convenção usada nos comentários de código

O código foi comentado pensando num leitor que conhece C mas está
aprendendo sistemas. Portanto:

- Comentários explicam **porquê** e **o que o hardware faz**, não apenas o quê.
- Muitas vezes ilustram o **formato da pilha/registradores** no comentário,
  porque bare-metal é isso: você gerencia a pilha e registradores à mão.
- Termos técnicos aparecem no **glossário** (`docs/glossario.md`).

Boa leitura — comece pelo `docs/01-conceitos.md`.