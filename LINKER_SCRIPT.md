# Explicação do Linker Script (kernel.ld)

## Visão Geral

O linker script `kernel.ld` define como o linker deve organizar as seções do código do kernel na memória. É essencial para kernels bare-metal porque:

1. Define o endereço base do kernel na memória
2. Organiza as seções de código e dados
3. Remove seções desnecessárias
4. Define símbolos para uso no código

## Estrutura Detalhada

### 1. Entry Point

```ld
ENTRY(_start)
```

- Define o ponto de entrada do kernel
- `_start` é a função que o bootloader chama para iniciar o kernel
- Deve ser definida no código C (ex: `void _start(void) { ... }`)

### 2. KERNEL_BASE

```ld
KERNEL_BASE = 0x100000;
```

- Endereço base onde o kernel é carregado na memória
- 0x100000 = 1MB (endereço padrão para kernels x86)
- O bootloader deve carregar o kernel neste endereço

### 3. Seções

#### .text (Código Executável)

```ld
.text KERNEL_BASE : AT(KERNEL_BASE)
{
    *(.text.boot)    /* Código de inicialização do bootloader */
    *(.text)         /* Código do kernel em C */
    *(.text.*)       /* Outras seções de código */
}
```

- Contém o código executável do kernel
- `.text.boot` é colocada primeiro (código de inicialização)
- O restante do código C vem depois

#### .rodata (Dados Somente Leitura)

```ld
.rodata : ALIGN(4096)
{
    *(.rodata)       /* Dados constantes */
    *(.rodata.*)     /* Outras seções de dados somente leitura */
}
```

- Contém dados constantes (strings, tabelas, etc.)
- Alinhada em 4096 bytes (tamanho de página)
- Não pode ser modificada durante a execução

#### .data (Dados Inicializados)

```ld
.data : ALIGN(4096)
{
    *(.data)         /* Dados inicializados */
    *(.data.*)       /* Outras seções de dados */
}
```

- Contém variáveis globais inicializadas
- Alinhada em 4096 bytes

#### .bss (Dados Não Inicializados)

```ld
.bss : ALIGN(4096)
{
    __bss_start = .; /* Início da área BSS */
    *(.bss)          /* Dados não inicializados */
    *(.bss.*)        /* Outras seções BSS */
    __bss_end = .;   /* Fim da área BSS */
}
```

- Contém variáveis globais não inicializadas
- Define símbolos `__bss_start` e `__bss_end` para uso no código
- Essencial para inicializar a memória BSS no startup

### 4. Seções Descartadas

```ld
/DISCARD/ :
{
    *(.eh_frame)     /* Informações de debug não necessárias */
    *(.comment)      /* Comentários do compilador */
    *(.note*)        /* Notas do ELF */
}
```

- Remove seções desnecessárias para reduzir o tamanho do kernel
- `.eh_frame` contém informações de exceção (não necessárias em bare-metal)
- `.comment` e `.note*` são metadados do compilador

## Uso no Código C

No código C, você pode usar os símbolos definidos no linker script:

```c
// Declaração externa dos símbolos
extern char __bss_start;
extern char __bss_end;

// Inicializar BSS
void init_bss(void) {
    char *bss_start = &__bss_start;
    char *bss_end = &__bss_end;
    
    while (bss_start < bss_end) {
        *bss_start++ = 0;
    }
}
```

## Flags do Linker

No Makefile, usamos:

```makefile
LDFLAGS = -T kernel.ld -nostdlib -nostartfiles -nodefaultlibs
```

- `-T kernel.ld`: Usa nosso linker script
- `-nostdlib`: Não usa bibliotecas padrão
- `-nostartfiles`: Não usa arquivos de inicialização padrão
- `-nodefaultlibs`: Não usa bibliotecas padrão

## Personalização

Para personalizar o linker script:

1. **Mudar endereço base**: Altere `KERNEL_BASE`
2. **Adicionar seções**: Adicione novas seções após `.bss`
3. **Mudar alinhamento**: Altere os valores `ALIGN(4096)`
4. **Adicionar símbolos**: Adicione novos símbolos como `__bss_start`

## Erros Comuns

1. **Entry point não encontrado**: Verifique se `_start` está definida no código C
2. **Símbolos indefinidos**: Verifique se os símbolos do linker script estão declarados como `extern` no código C
3. **Tamanho do kernel muito grande**: Verifique se está removendo seções desnecessárias no `/DISCARD/`