# 05 — Drivers de periféricos

> Arquivos: `kernel/drivers/vga.c`, `kernel/drivers/console.c`,
> `kernel/drivers/pit.c`, `kernel/drivers/keyboard.c`

Driver é o código que sabe conversar com um dispositivo específico. Aqui os
"dispositivos" são: tela (VGA), terminal (console), timer (PIT) e teclado
(PS/2).

## 5.1 VGA — a tela como memória (vga.c)

Em modo texto, a placa de vídeo mapeia a tela na memória em `0xB8000`.
Cada **célula** são 2 bytes:

```
BIT  15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
     ├── FUNDO ──┤ ├── FRENTE ─┤ ├── CARACTERE ASCII ─┤
        (4 bits)      (4 bits)        (8 bits)
```

- byte par  = caractere ASCII (0–255);
- byte ímpar = cor: nibble alto = fundo, nibble baixo = cor do texto.

O `vga_clear()` preenche as 2000 células com espaço em branco; o
`vga_putchar(x, y, c, color)` escreve numa linha/coluna exata; o
`vga_puts` faz o loop de string. Como acesso é direto à memória, o
compilador precisa dos acessos **volatile** (o hardware pode mudar a tela;
e nós não queremos que o gcc "otimize" os acessos).

```c
// kernel/drivers/vga.c
void vga_putchar(int x, int y, char c, unsigned char color) {
    int index = y * VGA_WIDTH + x;                 // célula → posição
    VGA_BUFFER[index] = (unsigned short)c | ((unsigned short)color << 8);
}
```

## 5.2 Console — a "janela" com cursor e scroll (console.c)

O console é uma **camada acima do VGA**: mantém a posição atual do cursor
(`cursor_x`, `cursor_y`) e implementa caracteres especiais (`\n`, `\b`,
`\t`), quebra de linha e **scroll**. Duas novidades aqui:

### Cursor de hardware

O próprio hardware de vídeo desenha o cursor piscante; a posição se
configura por portas (**portas 0x3D4/0x3D5** = CRTC):

```c
static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;   // índice linear 0..1999
    outb(0x3D4, 0x0F);  outb(0x3D5, pos & 0xFF);        // byte baixo
    outb(0x3D4, 0x0E);  outb(0x3D5, (pos >> 8) & 0xFF); // byte alto
}
```

O padrão `outb(porta, indice); outb(porta2, valor)` é típico do CRTC:
primeiro escolhe o **registrador** (0x0F = cursor low, 0x0E = cursor high),
depois escreve o **valor**.

### Scroll

Quando o cursor passa da última linha, o `scroll()` copia as linhas 1..24
para 0..23 (uma para cima) e limpa a última linha.

## 5.3 PIT — o timer que faz tudo acontecer (pit.c)

O **PIT 8253/8254** é um contador programável. Ele gera um pulso periódico,
que é ligado ao **IRQ0** do PIC. Programar é escrever em portas:

```c
#define PIT_CHANNEL0  0x40    // canal 0
#define PIT_COMMAND   0x43    // registro de modo
#define PIT_BASE_FREQ 1193182 // clk do PIT (1.19 MHz)

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQ / frequency;   // ex.: 1193182/100 ≈ 11931
    outb(PIT_COMMAND, 0x36);          // canal 0, bytes low+high, modo rate, binário
    outb(PIT_CHANNEL0, divisor & 0xFF);       // valor do contador (byte baixo)
    outb(PIT_CHANNEL0, divisor >> 8);         // valor do contador (byte alto)
}
```

Significado do `0x36` (modo): o bit 0 = BCD/off, bits 1–3 = modo 3
(rate generator), bits 4–5 = acessar low byte e depois high byte, bits 6–7 =
canal 0. Depois disso, o PIT puxa IRQ0 **100 vezes/segundo** e o vetor 32 da
IDT entra em ação — é o *pulsar* que move o escalonador (ver
`docs/06-escalonador.md`).

`pit_get_ticks()` devolve quantos ticks já passaram (incrementado em
`timer_tick` → `pit_tick_bump`). O terminal usa isso no `sys_get_tick` se
quiser.

## 5.4 Teclado PS/2 — de "código de tecla" a caractere (keyboard.c)

O teclado gera um **scancode** (um byte) em cada tecla pressionada e em cada
tecla solta. O IRQ1 dispara e o driver lê a porta `0x60`.

### Tabela de scancodes (Set 1, layout US)

O scancode **não é** o ASCII: `0x1E` = 'A', `0x10` = 'Q', etc. Por isso o
projeto tem **duas tabelas**:

```c
static const char scancode_low[SCANCODE_MAX];  // sem Shift
static const char scancode_shift[SCANCODE_MAX];// com Shift
```

### Bits especiais

- O bit 7 (`0x80`) marca **tecla solta**: `16` = apertou 'A'; `0x96` = soltou.
- Scancodes especiais: `0x2A`/`0x36` Shift, `0x3A` Caps Lock (apertar).
- Soltar Shift desliga o `shift_pressed`; soltar Caps Lock alterna `caps_lock`.

### O pipeline

```c
static void keyboard_handler(void) {       // roda na interrupção IRQ1
    uint8_t sc = inb(KEYBOARD_DATA);        // lê o byte do teclado
    if (sc & KEY_RELEASED) { ... }          // atalhos de Shift solto...
    ... map: char c = (shift) ? sc_shift[sc] : sc_low[sc];
    keybuf_push(c);                         // põe o char NA FILA
}
```

### Fila circular e leitura "sem travar"

`keybuf[128]` com `head`/`tail` é uma **fila circular**: o IRQ1 (veloz) só
escreve; o `keyboard_read()` (chamado pelo terminal) só lê. Se a fila
estiver cheia, perde a tecla. Isso protege o código de ler teclado no meio
da interrupção e resolve o problema de **consumidor produtor sem locks**
(num único core, "escritor no IRQ e leitor em syscall" basta).

Nota: `sys_read()` chamado pelo ring 3 → `kernel/syscall/syscall.c` chama
`keyboard_read()`.

## 5.5 Como um driver vira handler registrado?

`keyboard_init()` termina com:

```c
irq_register_handler(1, keyboard_handler);
```

Isso guarda a função num vetor `irq_handlers[16]` (do `idt.c`). Quando o
IRQ1 dispara, `isr_handler()` pega `irq_handlers[1]` e chama. O mesmo
mecanismo é usado para o timer — só que o timer **não passa pelo isr_handler**
(ver fluxo do IRQ0 em `docs/06-escalonador.md`).

## Próximo passo

Os periféricos funcionam. O próximo nível é o coração do sistema: o
escalonador — `docs/06-escalonador.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `kernel/drivers/vga.c` | memória mapeada + cor |
| `kernel/drivers/console.c` | cursor de hardware, scroll |
| `kernel/drivers/pit.c` | gravação do timer |
| `kernel/drivers/keyboard.c` | scancode → ASCII, fila, IRQ1 |
| `kernel/include/io.h` | `inb`/`outb` (todos os drivers usam) |