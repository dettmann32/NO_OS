# Kernel Bare-Metal x86_64

Kernel bare-metal organizado em pastas por função (bootloader, drivers, inicialização).

## Estrutura do Projeto

```
SO/
├── boot/
│   └── boot.asm              # Bootloader (modo real e protegido)
├── kernel/
│   ├── include/              # Headers públicos
│   │   ├── bss.h
│   │   ├── console.h
│   │   ├── gdt.h
│   │   ├── idt.h
│   │   ├── io.h
│   │   ├── keyboard.h
│   │   ├── pic.h
│   │   ├── pit.h
│   │   ├── fs.h
│   │   ├── registers.h
│   │   ├── syscall.h
│   │   ├── task.h
│   │   └── vga.h
│   ├── arch/
│   │   └── x86/
│   │       ├── gdt.c         # GDT (kernel/user), TSS e entrada no ring 3
│   │       ├── idt.c         # Tabela IDT e handlers de exceção/IRQ
│   │       ├── idt_stubs.asm # Stubs das interrupções + troca de contexto
│   │       └── pic.c         # Remapeamento do PIC 8259
│   ├── drivers/
│   │   ├── console.c         # Terminal VGA com cursor e scroll
│   │   ├── keyboard.c        # Driver PS/2 (scancode → ASCII, US)
│   │   ├── pit.c             # Timer 8253/8254 (IRQ0)
│   │   └── vga.c             # Driver de vídeo VGA (modo texto)
│   ├── fs/
│   │   └── fs.c              # Sistema de arquivos em RAM + presets de boot
│   ├── init/
│   │   └── bss.c             # Inicialização da seção BSS
│   ├── scheduler/
│   │   └── scheduler.c       # TCBs, round-robin e preempção pelo timer
│   ├── syscall/
│   │   └── syscall.c         # Syscalls via int 0x80 (SYS_WRITE/SYS_READ/...)
│   └── main.c                # Entry point do kernel
├── user/
│   └── user.c                # Programas que rodam em ring 3 (só syscalls)
├── linker/
│   └── kernel.ld             # Linker script
├── scripts/
│   ├── run_qemu.sh           # Executa o kernel no QEMU
│   ├── test_kernel.sh        # Compila e testa
│   └── final_test.sh         # Verificação do projeto
├── docs/                     # Documentação didática (parta do docs/README.md)
├── Makefile
└── build/                    # Artefatos de compilação (gerado)
```

## Recursos implementados

- **Escalonador**: troca de contexto preemptiva (round-robin) no IRQ0 do PIT a 100 Hz; cada tarefa tem pilha própria de kernel/user; TSS.esp0 atualizado a cada troca; tarefa ociosa (`hlt`)
- **Modo usuário (ring 3)**: GDT com segmentos de código/dados do usuário, transição via `iret` (CPL 3)
- **Interrupções**: IDT de 32 bits (48 entries), stubs em assembly para 32 exceções + 16 IRQs
- **PIC (8259)**: remapeado para vetores 0x20-0x2F (fora das exceções)
- **Timer (PIT)**: IRQ0 a 100 Hz, contador de ticks
- **Teclado (PS/2)**: IRQ1, scancode set 1 (layout US), Shift/Caps Lock, fila para `sys_read` (sem eco no kernel)
- **Syscalls**: dispatcher `int 0x80` com gate DPL 3 — `SYS_WRITE`, `SYS_READ`, `SYS_EXIT`, `SYS_GET_TICK`, `SYS_FILE_OPEN`, `SYS_FILE_READ`, `SYS_FILE_WRITE`, `SYS_FILE_CLOSE`, `SYS_FS_MKDIR`, `SYS_FS_CHDIR`, `SYS_FS_GETCWD`, `SYS_FS_RM`; chamadas do ring 3
- **Sistema de arquivos em RAM**: árvore de diretórios (dirs/arquivos com payload de até 508 bytes) populada no boot com `docs/`, `usr/`, `leiame.txt`, `inicio.txt`, `projetos/`, `bin/`; fds por tarefa
- **Terminal (ring 3)**: shell com `ls`, `cd`, `mkdir`, `touch`, `echo >`, `cat`, `rm`, `pwd`, `clear`, `help` e `exit`, executado como tarefa de usuário
- **Terminal VGA**: cursor de hardware, Enter, Backspace, Tab e scroll

## Como usar

> Para entender o código passo a passo, comece em **`docs/README.md`** —
> a documentação foi escrita para quem conhece C mas nunca programou
> bare-metal (11 arquivos, do bootloader ao terminal).

1. Compilar o kernel:
```bash
make
```

2. Rodar no QEMU:
```bash
make run
# ou
./scripts/run_qemu.sh
```

3. Limpar arquivos gerados:
```bash
make clean
```

## Estrutura do Linker Script

O `linker/kernel.ld` define:

- **Entry point**: `_start` (primeira função do binário, em `.text.boot`)
- **KERNEL_BASE**: 0x10000 (endereço onde o bootloader carrega o kernel)
- **Seções**: `.text`, `.rodata`, `.data`, `.user_text` (aplicação do ring 3), `.bss`
- **Símbolos**: `__bss_start` e `__bss_end` (para zerar o BSS)

## Compilação

O Makefile gera os artefatos em `build/`:

- `build/boot.bin` — bootloader (NASM)
- `build/kernel.bin` — kernel (gcc + ld + objcopy)
- `build/os.img` — imagem de disco completa (bootloader no setor 0, kernel no setor 1)

Flags do gcc: `-ffreestanding -fno-pie -fno-pic -fno-stack-protector -m32` (`-Ikernel/include`).