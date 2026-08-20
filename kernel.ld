/* Linker script para kernel bare-metal x86_64 */

/* Entry point - onde o kernel começa a executar */
ENTRY(_start)

/* Endereço base do kernel na memória (1MB = 0x100000) */
/* Este é o endereço padrão onde o bootloader carrega o kernel */
KERNEL_BASE = 0x100000;

SECTIONS
{
    /* Seção de código executável */
    .text KERNEL_BASE : AT(KERNEL_BASE)
    {
        /* Inclui o código do bootloader primeiro */
        *(.text.boot)    /* Código de inicialização do bootloader */
        *(.text)         /* Código do kernel em C */
        *(.text.*)       /* Outras seções de código */
    }

    /* Seção de dados somente leitura (constantes) */
    .rodata : ALIGN(4096)
    {
        *(.rodata)       /* Dados constantes */
        *(.rodata.*)     /* Outras seções de dados somente leitura */
    }

    /* Seção de dados inicializados */
    .data : ALIGN(4096)
    {
        *(.data)         /* Dados inicializados */
        *(.data.*)       /* Outras seções de dados */
    }

    /* Seção BSS (dados não inicializados) */
    .bss : ALIGN(4096)
    {
        __bss_start = .; /* Início da área BSS */
        *(.bss)          /* Dados não inicializados */
        *(.bss.*)        /* Outras seções BSS */
        __bss_end = .;   /* Fim da área BSS */
    }

    /* Remove seções desnecessárias */
    /DISCARD/ :
    {
        *(.eh_frame)     /* Informações de debug não necessárias */
        *(.comment)      /* Comentários do compilador */
        *(.note*)        /* Notas do ELF */
    }
}