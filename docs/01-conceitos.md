# 01 — Conceitos fundamentais de bare-metal

> Leia isto antes de qualquer código. É a base que um programador C num
> Linux/Windows nunca precisou saber porque o sistema operacional escondia
> tudo de você.

## 1.1 O que significa "bare-metal"

Quando você programa em C num Linux, o seu `main()` já executa com:

- Uma pilha pronta, memória mapeada, sistema de arquivos, `printf`, alocador...
- Tudo isso **fornecido pelo sistema operacional**.

Escribir um kernel é programar **o próprio sistema operacional**. O seu código é
o **primeiro** programa que roda no hardware. Não existe nada embaixo: nem C
runtime, nem `libc`, nem API de sistema. Se o seu código acessar um endereço
errado de memória, **ninguém vai te avisar** — apenas pode travar e reiniciar.

Este projeto roda em modo **32 bits** (i386), mesmo em CPUs modernas 64 bits,
para simplificar o endereçamento.

## 1.2 Os três mundos do hardware que o kernel conversa

Todo código de um kernel interage com o hardware por **três** caminhos:

| Caminho | Como funciona | Exemplo no projeto |
|---------|---------------|--------------------|
| **Instruções da CPU** | a CPU executa o seu código | `mov`, `add`, `int`, `iret`, `cli`, `hlt` |
| **Memória mapeada** | certos endereços de memória *são* periféricos | a tela VGA em `0xB8000` (`kernel/drivers/vga.c`) |
| **Portas de I/O** (`in`/`out`) | conversar com chips por portas x86 | timer PIT, PIC, teclado (`kernel/include/io.h`) |

### Memória mapeada — o exemplo da tela

Uma tela de texto VGA de 80x25 está "espelhada" na memória em `0xB8000`.
Cada célula da tela ocupa **2 bytes**: o caractere e sua cor.

```
0xB8000: [char][cor] [char][cor] ...
          cel0        cel1    →  80*25 = 2000 células = 4000 bytes
```

Por isso em `kernel/drivers/vga.c` é tão simples "desenhar":

```c
#define VGA_BUFFER ((volatile unsigned short *)0xB8000)
VGA_BUFFER[0] = (unsigned short)'K' | (15 << 8);  // 'K' branco na célula 0
```

O `volatile` impede o compilador de "otimizar" esses acessos, pois a memória
pode mudar sem o código saber (é o hardware escrevendo).

### Portas de I/O

Alguns periféricos não são memória: falam por **portas**. O x86 tem 65536
portas de 8/16/32 bits e as instruções `in`/`out` para ler/escrever nelas.
O projeto encapsula isso em `kernel/include/io.h`:

```c
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) { ... }
```

É assim que se programa o timer, o PIC, o teclado e o cursor de hardware — em
`kernel/drivers/*.c`.

## 1.3 O que acontece quando o PC "liga" (reset)

1. A CPU começa executando a partir do endereço físico `0xFFFF0` — mas o
   **BIOS** já está lá (uma ROM com código de fábrica). Na verdade a CPU
   executa o BIOS.
2. O BIOS **testa o hardware** (POST), **inicializa** memória/teclado/HD.
3. O BIOS lê **o primeiro setor (512 bytes)** do disco de boot (no QEMU, o
   nosso disquete) para o endereço `0x7C00` e **pula para ele**.
4. Nesse momento a CPU está em **modo real** (16 bits, sem proteção).
5. **Nosso** código entra em cena: é o `boot/boot.asm`.

> O bootloader não é o kernel: é um pequeno programa (512 bytes!) que
> carrega o kernel maior (32 KiB) para a memória e salta para ele.

## 1.4 Modo real vs. modo protegido

| | Modo real (16 bits) | Modo protegido (32 bits) |
|---|---|---|
| Tamanho de registradores | 16 bits (AX, BX...) | 32 bits (EAX, EBX...) |
| Endereço máximo acessível | 1 MiB (segmento:offseto) | 4 GiB |
| Proteção | nenhuma (código pode mexer em tudo) | anéis de privilégio, GDT |
| Quem usa | BIOS, bootloader antes de entrar | o kernel, aplicações |

O **bootloader** faz a transição: no começo lê disco via BIOS (que exige modo
real/16 bits), depois salta para o modo protegido e entrega o controle ao
kernel C.

## 1.5 O que é "endereço linear" / "endereço físico"

- **Endereço físico**: o número real do byte na RAM (ex.: `0xB8000`).
- **Endereço linear**: quantidade calculada pela CPU a partir de segmento +
  offseto. Neste projeto **não há paginação**, então linear == físico.

O formato **segmento:offseto** do modo: `offset + segmento_em_para * 16`.
Ex.: seg `0x1000`, offset `0x0000` → `0x10000` (onde o kernel é carregado).

## 1.6 Anéis de proteção (ring 0 / ring 3)

A CPU x86 em modo protegido roda com 4 níveis de privilégio: ring 0 (máximo)
até ring 3 (mínimo). O **anel define o que o código pode fazer**:

| Anel | Pode | Não pode |
|------|------|----------|
| 0 (kernel) | tudo: portas, instruções privilegiadas, interromper fluxo | — |
| 3 (usuário) | usar o que o kernel expõe via syscall | `in`/`out`, mexer na IDT/GDT, mudar anel |

O kernel roda no anel 0. A aplicação (o terminal) roda no anel 3. Quando a
aplicação precisa de algo (escrever na tela, ler teclado, acessar arquivos),
ela usa **syscall** — pede ao kernel, que faz em name dela e devolve o
resultado. Isso é **exatamente** como funciona o Linux com `write()`,
`open()` etc.

## 1.7 GDT, IDT e TSS – as "tabelas mágicas"

São apenas **estruturas de dados na memória** que a CPU consulta:

- **GDT** (Global Descriptor Table): define segmentos e seus anéis. A CPU
  olha para ela a cada acesso de memória para saber se você *pode* acessar.
- **IDT** (Interrupt Descriptor Table): lista o que fazer para cada sinal de
  interrupção (timer, teclado, erro de divisão...). Quando um sinal chega, a
  CPU olha a IDT e pula para o handler correspondente.
- **TSS** (Task State Segment): uma pequena área que guarda onde está a
  **pilha do kernel** (*esp0*) — usada quando uma interrupção chega com o
  código rodando no anel 3.

Não é magia: são tabelas preenchidas em C (veja `kernel/arch/x86/*.c`) e a
CPU tem instruções específicas (`lgdt`, `lidt`, `ltr`) para "apontar" para
elas.

## 1.8 Interrupções

Uma interrupção é um *sinal* que faz o processador **parar** o que estava
fazendo, **salvar o contexto** e executar uma rotina (handler). Origem:

- **Hardware** (IRQ): o timer (100x/s), o teclado (toda tecla), etc.
- **Software** (exceções e syscalls): `int 0x80` do ring 3, divisão por zero.

Cada origem tem um **número de vetor**. A interrupção *mapeia* o vetor para
um endereço na IDT → é por isso que existe a tabela.

No esquema a seguir, o timer gera uma "batida" 100 vezes por segundo; o
escalonador usa isso para rodar cada tarefa em rodízio:

```
timer PIT ──> PIC ("é a interrupção IRQ0") ──> CPU olha IDT[32]
   ──> pula para o handler ──> (processa) ──> volta ao ponto de onde parou
```

## 1.9 O que este projeto NÃO tem (e por quê)

- **Sem libc / sem `malloc`**: a memória é distribuída em arrays estáticos
  globais (veja `kernel/scheduler/scheduler.c`: pilhas de cada tarefa).
- **Sem paginação**: endereço linear = físico. Simplifica muita coisa.
- **Sem driver de vídeo legado**: apenas modo texto 80x25 (simples e robusto).
- **Sem gerenciamento de discos reais**: o "arquivo" mora na memória RAM do
  kernel (sistema de arquivos em RAM).

## Próximo passo

Agora que você sabe o que é "hardware do ponto de vista do kernel", veja como
tudo começa: `docs/02-boot.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `kernel/include/io.h` | `inb`/`outb`/`io_wait` (portas) |
| `kernel/include/vga.h` | `VGA_BUFFER` (memória mapeada) |
| `kernel/drivers/vga.c` | uso da memória mapeada |
| `boot/boot.asm` | começo de tudo (modo real) |