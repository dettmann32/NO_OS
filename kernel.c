/* Kernel bare-metal x86_64 simples */

#define VGA_COLS 80
#define VGA_ROWS 25

/* Limpa a tela inteira preenchendo com espaços */
void clear_screen(void) {
    volatile unsigned short *video = (unsigned short *)0xB8000;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        video[i] = (unsigned short)' ' | 0x0F00;
    }
}

/* Função para escrever na tela usando VGA */
void kprint(const char *str) {
    volatile unsigned short *video = (unsigned short *)0xB8000;
    while (*str) {
        *video++ = (unsigned short)*str++ | 0x0F00; /* Branco sobre preto */
    }
}

/* Entry point do kernel */
/* Deve ser a PRIMEIRA função do binário: o bootloader pula para 0x10000 */
__attribute__((section(".text.boot"), used))
void _start(void) {
    clear_screen();
    kprint("Kernel carregado !!!");
    
    /* Loop infinito para evitar que o kernel termine */
    while (1) {
        /* Halt - espera por interrupção */
        __asm__ __volatile__("hlt");
    }
}