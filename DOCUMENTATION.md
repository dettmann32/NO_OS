# Documentação do Projeto Kernel Bare-Metal x86_64

## Visão Geral

Este projeto implementa um kernel bare-metal simples para x86_64 com um linker script para linkar o código C com o bootloader. O projeto inclui:

1. **Bootloader** (`boot.asm`) - Código em Assembly que inicializa o processador
2. **Kernel** (`kernel.c` e `kernel_example.c`) - Código em C do kernel
3. **Linker Script** (`kernel.ld`) - Define como o kernel é organizado na memória
4. **Makefile** - Automatiza a compilação e linkagem

## Arquivos do Projeto

### Arquivos Principais

| Arquivo | Descrição |
|---------|-----------|
| `boot.asm` | Bootloader em Assembly para x86_64 |
| `boot.bin` | Bootloader compilado (binário) |
| `kernel.c` | Kernel simples em C |
| `kernel_example.c` | Kernel exemplo completo em C |
| `kernel.ld` | Linker script para x86_64 |
| `Makefile` | Makefile para compilar o kernel |

### Arquivos de Documentação

| Arquivo | Descrição |
|---------|-----------|
| `README.md` | Guia rápido de uso |
| `LINKER_SCRIPT.md` | Explicação detalhada do linker script |
| `DOCUMENTATION.md` | Esta documentação completa |

### Scripts de Execução

| Arquivo | Descrição |
|---------|-----------|
| `run_qemu.sh` | Script para testar o kernel com QEMU |
| `test_kernel.sh` | Script completo de teste |
| `run_example.sh` | Script para testar o kernel exemplo |

## Como Usar

### 1. Compilar o Kernel

```bash
make
```

Isso irá compilar:
- `kernel.bin` - Kernel simples
- `kernel_example.bin` - Kernel exemplo completo

### 2. Testar com QEMU

```bash
# Testar kernel simples
./run_qemu.sh

# Testar kernel exemplo
./run_example.sh

# Teste completo
./test_kernel.sh
```

### 3. Limpar Arquivos Gerados

```bash
make clean
```

## Estrutura do Projeto

```
SO/
├── boot.asm              # Bootloader em Assembly
├── boot.bin              # Bootloader compilado
├── kernel.c              # Kernel simples em C
├── kernel_example.c      # Kernel exemplo completo em C
├── kernel.ld             # Linker script para x86_64
├── Makefile              # Makefile para compilar o kernel
├── README.md             # Guia rápido de uso
├── LINKER_SCRIPT.md      # Explicação do linker script
├── DOCUMENTATION.md      # Esta documentação
├── run_qemu.sh           # Script para testar com QEMU
├── test_kernel.sh        # Script de teste completo
└── run_example.sh        # Script para testar kernel exemplo
```

## Componentes Principais

### 1. Linker Script (`kernel.ld`)

O linker script define:

- **Entry point**: `_start` (ponto de entrada do kernel)
- **KERNEL_BASE**: 0x100000 (1MB, endereço onde o bootloader carrega o kernel)
- **Seções**:
  - `.text` - Código executável
  - `.rodata` - Dados somente leitura
  - `.data` - Dados inicializados
  - `.bss` - Dados não inicializados
- **Símbolos**: `__bss_start` e `__bss_end` para uso no código C

### 2. Kernel Simples (`kernel.c`)

Implementa:
- Função `kprint()` para escrever na tela usando VGA
- Entry point `_start()` que exibe uma mensagem
- Loop infinito com `hlt` para evitar término

### 3. Kernel Exemplo (`kernel_example.c`)

Implementa:
- Inicialização BSS usando símbolos do linker script
- Funções VGA completas (clear, puts, putchar)
- Mensagem de boas-vindas colorida
- Inicialização completa do kernel

### 4. Bootloader (`boot.asm`)

Implementa:
- Inicialização em modo real (16 bits)
- Mudança para modo protegido (32 bits)
- Configuração da GDT (Global Descriptor Table)
- Carregamento do kernel em 0x100000

## Compilação e Flags

### Compilador (gcc)

```makefile
CC = gcc
CFLAGS = -ffreestanding -O2 -Wall -Wextra -m64 -c
```

- `-ffreestanding`: Compila sem bibliotecas padrão
- `-O2`: Otimização nível 2
- `-Wall -Wextra`: Habilita todos os warnings
- `-m64`: Gera código para 64 bits
- `-c`: Compila mas não linka

### Linker (ld via gcc)

```makefile
LDFLAGS = -T kernel.ld -nostdlib -nostartfiles -nodefaultlibs
```

- `-T kernel.ld`: Usa nosso linker script
- `-nostdlib`: Não usa bibliotecas padrão
- `-nostartfiles`: Não usa arquivos de inicialização padrão
- `-nodefaultlibs`: Não usa bibliotecas padrão

## Personalização

### Mudar Endereço Base do Kernel

Edite `kernel.ld`:

```ld
KERNEL_BASE = 0x200000;  // 2MB em vez de 1MB
```

### Adicionar Nova Seção

Adicione no `kernel.ld`:

```ld
.my_section : ALIGN(4096)
{
    *(.my_section)
    *(.my_section.*)
}
```

### Usar Símbolos no Código C

No código C, declare:

```c
extern char __bss_start;
extern char __bss_end;
```

Use-os conforme necessário (ex: inicialização BSS).

## Solução de Problemas

### Erro: "Entry point not found"

Verifique se `_start` está definida no código C:

```c
void _start(void) {
    // código do kernel
}
```

### Erro: "Undefined symbols"

Verifique se os símbolos do linker script estão declarados como `extern` no código C.

### Erro: "Kernel too big"

Verifique se está removendo seções desnecessárias no `/DISCARD/` do linker script.

### Kernel não inicia no QEMU

1. Verifique se `boot.bin` está correto
2. Verifique se o kernel está sendo carregado no endereço correto
3. Use `objdump -h kernel.bin` para verificar as seções

## Próximos Passos

1. **Interrupções**: Implementar handler de interrupções
2. **Memória**: Implementar gerenciamento de memória (heap, pilha)
3. **Drivers**: Implementar drivers básicos (teclado, disco)
4. **Sistema de arquivos**: Implementar sistema de arquivos simples
5. **Processos**: Implementar gerenciamento de processos

## Referências

- [OSDev Wiki](https://wiki.osdev.org/)
- [Writing an OS in Rust](https://os.phil-opp.com/)
- [James Molloy's Kernel Tutorial](http://www.jamesmolloy.co.uk/tutorial_html/)
- [OSDev.org Linker Scripts](https://wiki.osdev.org/Linker_Scripts)