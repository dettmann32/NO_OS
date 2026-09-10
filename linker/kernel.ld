/* Linker script para kernel bare-metal x86_64 */

/* Entry point - onde o kernel começa a executar */
ENTRY(_start)

/* Endereço base do kernel na memória (64KB = 0x10000) */
/* Deve bater com KERNEL_OFFSET em boot.asm */
KERNEL_BASE = 0x10000;

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

    /* Código da aplicação que roda em modo usuário (ring 3) */
    .user_text ALIGN(4096) :
    {
        *(.user_text)    /* Aplicação do usuário */
        *(.user_text.*)  /* Outras seções da aplicação */
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