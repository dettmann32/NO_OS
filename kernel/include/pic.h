#ifndef PIC_H
#define PIC_H

/*
 * PIC 8259 (kernel/arch/x86/pic.c): portas e constantes usadas
 * para REMAPE das IRQs para os vetores 32-47:
 *   - PIC1_OFFSET 0x20 → IRQ0-7  viram vetores 0x20-0x27
 *   - PIC2_OFFSET 0x28 → IRQ8-15 viram vetores 0x28-0x2F
 * PIC_EOI 0x20 é o comando "fim de interrupção" (obrigatório ao
 * terminar de atender um IRQ, senão o PIC não re-libera a linha).
 */

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20
#define PIC1_OFFSET  0x20
#define PIC2_OFFSET  0x28

void pic_remap(void);
void pic_send_eoi(unsigned char irq);

#endif