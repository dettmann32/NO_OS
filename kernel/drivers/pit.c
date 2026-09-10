#include <stdint.h>
#include "../include/pit.h"
#include "../include/io.h"

/*
 * ============================================================
 * PIT — Programmable Interval Timer (8253/8254)
 * ============================================================
 * O timer gera um "pulso" periódico que fica ligado ao IRQ0 do
 * PIC. A cada pulso, o processador é interrompido (vetor 32) e
 * o escalonador aproveita para trocar de tarefa. É literalmente
 * o "coração" da preempção / multitarefa.
 *
 * Programar é escrever em portas de I/O:
 *   - 0x43 = registro de modo (como vai contar)
 *   - 0x40 = canal 0 (onde colamos o valor do divisor)
 *
 * Texto didático completo: docs/05-drivers.md
 * ============================================================
 */

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_FREQ 1193182        /* clock do PIT: 1.193182 MHz */

static volatile uint32_t ticks = 0;  /* contador; incrementado no IRQ0 */

/* Contador de ticks incrementado pelo escalonador no IRQ0 */
void pit_tick_bump(void) {
    ticks++;
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQ / frequency;   /* 1193182/100 ≈ 11931 */

    /* 0x36 = 00│011│0│1│10:
     *   bits 7-6 = canal 0
     *   bits 5-4 = acessar low byte depois high byte
     *   bits 3-1 = modo 3 (rate generator / onda quadrada)
     *   bit  0   = binário (não BCD)
     * Em seguida escreve o divisor (2 bytes) no canal 0. */
    outb(PIT_COMMAND, 0x36);            /* canal 0, lobyte/hibyte, gerador de taxa */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t pit_get_ticks(void) {
    return ticks;
}