#include "../../include/pic.h"
#include "../../include/io.h"

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

    /* Máscara: IRQ0 (timer) e IRQ1 (teclado) liberados; resto mascarado */
    outb(PIC1_DATA, 0xFC);      /* 1111 1100 */
    outb(PIC2_DATA, 0xFF);      /* 1111 1111 */
}

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}