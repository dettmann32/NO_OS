/* Kernel bare-metal x86_64 completo */

// Declaração externa dos símbolos do linker script
extern char __bss_start;
extern char __bss_end;

// Endereço do buffer VGA (modo texto)
volatile unsigned short *VGA_BUFFER = (unsigned short *)0xB8000;

// Tamanho da tela
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Cores do VGA
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_WHITE 15
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_RED 4

// Função para converter posição (x,y) para índice do buffer VGA
int vga_index(int x, int y) {
    return y * VGA_WIDTH + x;
}

// Função para escrever caractere na tela
void vga_putchar(int x, int y, char c, unsigned char color) {
    int index = vga_index(x, y);
    VGA_BUFFER[index] = (unsigned short)c | ((unsigned short)color << 8);
}

// Função para escrever string na tela
void vga_puts(int x, int y, const char *str, unsigned char color) {
    int i = 0;
    while (str[i] != '\0') {
        vga_putchar(x + i, y, str[i], color);
        i++;
    }
}

// Função para limpar a tela
void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = (unsigned short)' ' | ((unsigned short)VGA_COLOR_WHITE << 8);
    }
}

// Função para inicializar BSS
void init_bss(void) {
    char *bss_start = &__bss_start;
    char *bss_end = &__bss_end;
    
    while (bss_start < bss_end) {
        *bss_start++ = 0;
    }
}

// Entry point do kernel
// Deve ser a PRIMEIRA função do binário: o bootloader pula para 0x10000
__attribute__((section(".text.boot"), used))
void _start(void) {
    // Inicializar BSS
    init_bss();
    
    // Limpar a tela
    vga_clear();
    
    // Escrever mensagem de boas-vindas
    vga_puts(10, 10, "Kernel Bare-Metal x86_64", VGA_COLOR_GREEN);
    vga_puts(10, 12, "Linker script funcionando!", VGA_COLOR_WHITE);
    vga_puts(10, 14, "Pressione Ctrl+C para reiniciar", VGA_COLOR_RED);
    
    // Loop infinito para evitar que o kernel termine
    while (1) {
        // Halt - espera por interrupção
        __asm__ __volatile__("hlt");
    }
}