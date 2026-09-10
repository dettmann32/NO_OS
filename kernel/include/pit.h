#ifndef PIT_H
#define PIT_H

/*
 * PIT (kernel/drivers/pit.c): timer de hardware do IRQ0. A cada
 * tick (padrão: 100 Hz) o escalonador troca de tarefa — é a FONTE
 * de tempo do sistema. pit_tick_bump() é chamado no timer_tick.
 */

#include <stdint.h>

void pit_init(uint32_t frequency);
void pit_tick_bump(void);
uint32_t pit_get_ticks(void);

#endif