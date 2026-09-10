#include <stdint.h>
#include "../../include/gdt.h"

/*
 * ============================================================
 * GDT (Global Descriptor Table) + TSS + entrada no ring 3
 * ============================================================
 * A GDT é uma tabela de 8 bytes/entrada que define segmentos de
 * memória e seus níveis de privilégio. Todo acesso a memória no
 * modo protegido passa por ela.
 *
 * Entradas usadas neste projeto (seletores em kernel/include/gdt.h):
 *   [0] null        — entrada 0 é obrigatória zerada
 *   [1] 0x08 code   — kernel, anel 0
 *   [2] 0x10 data   — kernel, anel 0
 *   [3] 0x1B code   — usuário, anel 3  (terminal)
 *   [4] 0x23 data   — usuário, anel 3
 *   [5] 0x28 TSS    — Task State Segment (pilha do kernel do user)
 *
 * Texto didático completo: docs/04-gdt-idt-pic.md
 * ============================================================
 */

/*
 * Um descritor de segmento (8 bytes, SEM padding - o packing é
 * essencial porque a CPU espera exatamente 8 bytes seguidos).
 * O formato é fragmentado por herança histórica do x86:
 * limite/base são partidos em pedaços.
 */
struct gdt_entry {
    uint16_t limit_low;        /* limite  bits 0-15  */
    uint16_t base_low;         /* base    bits 0-15  */
    uint8_t base_mid;          /* base    bits 16-23 */
    uint8_t access;            /* privilégio + tipo (o "anel") */
    uint8_t flags_lim_high;    /* granularidade + limite bits 16-19 */
    uint8_t base_high;         /* base    bits 24-31 */
} __attribute__((packed));

/* Ponteiro usado pela instrução lgdt: tamanho + endereço da tabela. */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/*
 * TSS (Task State Segment): a CPU lê daqui onde ficam o segmento
 * (ss0) e o topo (esp0) da pilha do KERNEL quando recebe uma
 * interrupção vinda do anel 3. Precisamos disso para trocar do
 * núcleo do processador para as tasks.
 */
struct tss {
    uint32_t prev_tss;
    uint32_t esp0;             /* ← topo da pilha do kernel (interrupção) */
    uint32_t ss0;              /* ← segmento da pilha do kernel */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap;
} __attribute__((packed));

static struct gdt_entry gdt[6];      /* a tabela em si */
static struct gdt_ptr gdtp;          /* o "ponteiro" para a lgdt */
static struct tss tss;               /* o TSS (instanciado) */

/* Pilhas globais usadas quando o kernel "entra" no ring 3 e para o
 * handler de interrupções. (O escalonador tem as pilhas POR TAREFA;
 * estas são pilhas fixas de inicialização.) */
static uint8_t kernel_stack[8192] __attribute__((aligned(16)));
static uint8_t user_stack[4096] __attribute__((aligned(16)));

/* Preenche uma entrada da GDT montando os campos fragmentados. */
static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
    gdt[idx].base_low = base & 0xFFFF;
    gdt[idx].base_mid = (base >> 16) & 0xFF;
    gdt[idx].base_high = (base >> 24) & 0xFF;
    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].flags_lim_high = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    gdt[idx].access = access;
}

void gdt_init(void) {
    /* Access byte (de cima para baixo na GDT):
     * 0x9A = 1001 1010b → anel 0, executável (code)
     * 0x92 = 1001 0010b → anel 0, data
     * 0xFA = 1111 1010b → anel 3 (bits 6-5 = 11), executável
     * 0xF2 = 1111 0010b → anel 3, data
     * Granularidade 0xCF: lim 4 GiB (0xFFFFF * 4096), 32 bits.
     * TSS: access 0x89 (presente, anel 0, "busy"). */
    gdt_set_entry(0, 0, 0, 0, 0);                  /* null */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);      /* kernel code  (ring 0) */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);      /* kernel data  (ring 0) */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);      /* user code    (ring 3) */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);      /* user data    (ring 3) */
    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x00); /* TSS */

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint32_t)&gdt;

    /* lgdt: diz à CPU onde a GDT está. Daqui em diante TODA troca
     * de segmento passa pela tabela (e é quando os anéis ganham
     * significado). */
    __asm__ volatile("lgdt %0" : : "m"(gdtp));

    /* O bootloader já tinha carregado uma GDT simples; agora que a
     * nossa (com mais entradas) está ativa, recarregamos os segmentos
     * de dados usando o novo seletor de kernel (0x10). CS continua
     * 0x08 (não mudou de privilégio, então o CPU mantém). */
    __asm__ volatile(
        "movw $0x10, %%ax;"
        "movw %%ax, %%ds;"
        "movw %%ax, %%es;"
        "movw %%ax, %%fs;"
        "movw %%ax, %%gs;"
        "movw %%ax, %%ss;"
        : : : "ax");
}

void tss_init(void) {
    /* ss0/esp0: segmento e topo da pilha do kernel usados quando
     * uma interrupção chega com o código rodando no ring 3. */
    tss.ss0 = KERNEL_DS;
    tss.esp0 = (uint32_t)kernel_stack + sizeof(kernel_stack);

    /* Atualiza a entrada da GDT e marca a TSS como ativa.
     * ltr ("load task register") grava o seletor da TSS corrente. */
    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x00);
    __asm__ volatile("ltr %%ax" : : "a"(TSS_SEL));
}

/* O escalonador chama isto a cada troca de tarefa: o topo da pilha
 * do kernel muda para a pilha da tarefa que passa a correr. Sem isso,
 * a próxima interrupção do ring 3 empilharia na pilha errada. */
void tss_set_esp0(uint32_t esp0) {
    tss.esp0 = esp0;
}

/*
 * Entrada no anel 3 (não usado no boot atual - o escalonador já
 * cria a primeira tarefa de user - mas útil como exemplo didático
 * do mecanismo iret).
 *
 * O truque: montar na pilha do kernel o "frame" que a CPU
 * restauraria num iret: SS, ESP, EFLAGS, CS, EIP. O iret então
 * "acorda" o anel 3, executando entry com a pilha de usuário.
 */
__attribute__((noreturn))
void enter_usermode(void (*entry)(void)) {
    uint32_t user_stack_top = (uint32_t)user_stack + sizeof(user_stack);

    /* Monta o frame de iret: SS, ESP, EFLAGS, CS, EIP */
    __asm__ volatile(
        "movl %0, %%esp;"
        "pushl %1;"             /* SS  (user data)   */
        "pushl %0;"             /* ESP (user stack)  */
        "pushl $0x202;"         /* EFLAGS (IF=1)     */
        "pushl %2;"             /* CS  (user code)   */
        "pushl %3;"             /* EIP (entry)       */
        "iret;"
        :
        : "r"(user_stack_top), "r"(USER_DS), "r"(USER_CS), "r"(entry)
        : "memory");

    for (;;) {
    }
}