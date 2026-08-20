/* Kernel bare-metal x86_64 simples */

/* Função para escrever na tela usando VGA */
void kprint(const char *str) {
    volatile unsigned short *video = (unsigned short *)0xB8000;
    while (*str) {
        *video++ = (unsigned short)*str++ | 0x0F00; /* Branco sobre preto */
    }
}

/* Entry point do kernel */
void _start(void) {
    kprint("Kernel carregado com sucesso!");
    
    /* Loop infinito para evitar que o kernel termine */
    while (1) {
        /* Halt - espera por interrupção */
        __asm__ __volatile__("hlt");
    }
}