#include "../../include/pic.h"
#include "../../include/io.h"

/*
 * ============================================================
 * PIC 8259 - "gateway" das interrupções de hardware
 * ============================================================
 * O PIC é o chip que recebe os IRQs dos periféricos (timer IRQ0,
 * teclado IRQ1, ...) e decide qual vetor de interrupção a CPU vai
 * receber.
 *
 * PROBLEMA INICIAL: por padrão ele mapeia IRQ0-7 nos vetores 0-7,
 * que COLIDEM com as exceções do processador (divisão por zero
 * etc). Por isso fazemos o "remap": IRQ0-15 passam a gerar
 * vetores 32-47 (fora da faixa das exceções).
 *
 * Texto didático completo: docs/04-gdt-idt-pic.md
 * (portas definidas em kernel/include/pic.h)
 * ============================================================
 */

/*
 * Initialization Command Words (ICW): uma sequência de bytes
 * escritos pelas portas de comando/dados que configura os dois
 * PICs (master 0x20/0x21, slave 0xA0/0xA1).
 *
 *   ICW1 (0x11) = iniciar e esperar ICW4
 *   ICW2 (0x20/0x28) = vetor base: IRQ0-7→32-39, IRQ8-15→40-47
 *   ICW3 (0x04/0x02) = cascata: slave ligado na linha 2 do master
 *   ICW4 (0x01) = modo 8086 (x86 moderno)
 *
 * `io_wait()` dá um pequeno atraso para o chip processar (porta 0x80).
 */
void pic_remap(void) {
    outb(PIC1_COMMAND, 0x11);   /* ICW1: inicia, espera ICW4 */
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    outb(PIC1_DATA, PIC1_OFFSET);   /* ICW2: vetores 0x20-0x27 */
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);   /* ICW2: vetores 0x28-0x2F */
    io_wait();

    outb(PIC1_DATA, 0x04);      /* ICW3: master tem escravo no IRQ2 */
    io_wait();
    outb(PIC2_DATA, 0x02);      /* ICW3: escravo ligado na linha 2 */
    io_wait();

    outb(PIC1_DATA, 0x01);      /* ICW4: modo 8086 */
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Máscara de IRQs: bit = 1 desliga aquele IRQ.
     * Aqui só IRQ0 (timer) e IRQ1 (teclado) ficam ligados
     * (0xFC = 1111 1100); todo o slave é mascarado. */
    outb(PIC1_DATA, 0xFC);      /* 1111 1100 */
    outb(PIC2_DATA, 0xFF);      /* 1111 1111 */
}

/*
 * EOI - End Of Interrupt. Obrigatório depois de atender um IRQ:
 * se não avisarmos o PIC, ele fica "segurando" a linha e NUNCA
 * mais deixa chegar outra interrupção daquele dispositivo.
 * Para IRQs vindos do slave (>= 8) é preciso EOI nos dois PICs.
 */
void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}