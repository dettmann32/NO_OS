# Kernel Bare-Metal x86_64

Este projeto contém um kernel bare-metal simples para x86_64 com um linker script para linkar o código C com o bootloader.

## Arquivos

- `boot.asm` - Bootloader em Assembly para x86_64
- `boot.bin` - Bootloader compilado (binário)
- `kernel.c` - Kernel em C
- `kernel.ld` - Linker script para x86_64
- `Makefile` - Makefile para compilar o kernel

## Como usar

1. Compilar o kernel:
```bash
make
```

2. Limpar arquivos gerados:
```bash
make clean
```

3. Testar com QEMU:
```bash
qemu-system-x86_64 -drive file=boot.bin,format=raw
```

## Estrutura do Linker Script

O `kernel.ld` define:

- **Entry point**: `_start` (ponto de entrada do kernel)
- **KERNEL_BASE**: 0x100000 (1MB, endereço onde o bootloader carrega o kernel)
- **Seções**:
  - `.text` - Código executável
  - `.rodata` - Dados somente leitura
  - `.data` - Dados inicializados
  - `.bss` - Dados não inicializados

## Compilação

O Makefile usa:
- `gcc` com flags `-ffreestanding -O2 -Wall -Wextra -m64 -c`
- Linker script `-T kernel.ld`
- Flags de linkagem `-nostdlib -nostartfiles -nodefaultlibs`