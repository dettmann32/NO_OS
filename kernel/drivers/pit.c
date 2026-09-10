#include <stdint.h>
#include "../include/pit.h"
#include "../include/io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_FREQ 1193182

static volatile uint32_t ticks = 0;

/* Contador de ticks incrementado pelo escalonador no IRQ0 */
void pit_tick_bump(void) {
    ticks++;
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQ / frequency;

    outb(PIT_COMMAND, 0x36);            /* canal 0, lobyte/hibyte, gerador de taxa */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t pit_get_ticks(void) {
    return ticks;
}