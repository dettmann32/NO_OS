# 03 — Build: como o código vira uma imagem de disco

> Arquivos: `Makefile` e `linker/kernel.ld`

Em C "normal" você faz `gcc main.c -o app` e pronto — o sistema operacional
cuida do resto. Num kernel, o **Makefile** precisa fazer tudo à mão,
porque não existe sistema para carregar o executável.

## 3.1 As ferramentas

| Ferramenta | Função neste projeto |
|------------|----------------------|
| `nasm` | monta o `boot.asm` (→ `boot.bin`, binário puro de 512 bytes) e o `idt_stubs.asm` (→ objeto ELF, para linkar com o C) |
| `gcc` | compila cada `.c` para objeto `.o` (32 bits, *freestanding*) |
| `ld` | junta todos os `.o` num `kernel.elf`, **usando nosso linker script** |
| `objcopy` | converte o `.elf` para **binário puro** (`kernel.bin`) — sem cabeçalhos/relocações |
| `dd`/`truncate` | montam a imagem de disquete: bootloader no setor 0, kernel no setor 1 |

## 3.2 Flags do gcc — o essencial das opções

```makefile
CFLAGS = -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
         -m32 -mno-mmx -mno-sse -mno-sse2 -O2 -Wall -Wextra -c
```

| Flag | Por quê |
|------|---------|
| `-ffreestanding` | compilar sem supor que existe libc; o compilador não injeta código de "runtime" |
| `-fno-pie -fno-pic` | não gerar código relocável/independente de posição (vamos fixar endereços absolutos) |
| `-fno-stack-protector` | não injetar *canaries* de pilha (chamariam funções de libc que não existem) |
| `-fno-builtin` | não substituir nossas funções por versões internas do gcc |
| `-m32` | **32 bits** (IA-32) |
| `-mno-mmx -mno-sse -mno-sse2` | não emitir instruções vetoriais (evita salvamento de estado FPU no contexto) |
| `-O2` | otimização |
| `-Wall -Wextra` | avisos |
| `-c` | gerar objeto, sem linkar |

A chave é: **o kernel é um programa que roda sem nada embaixo**, então o
compilador não pode gerar chamadas para `memcpy`, `memset`, proteções etc.

## 3.3 O linker script — onde cada coisa mora na memória

O `linker/kernel.ld` diz ao `ld` **em que endereço** cada seção deve ficar
no arquivo binário final. É fácil entender olhando:

```ld
KERNEL_BASE = 0x10000;       /* endereço onde o bootloader carrega o kernel */

SECTIONS {
    .text KERNEL_BASE : AT(KERNEL_BASE) { ... }   /* código executável */
    .rodata : AT(...)  { ... }                     /* constantes (string literais) */
    .data   : AT(...)  { ... }                     /* globais inicializados */
    .user_text AT(...) { ... }                     /* código da aplicação de ring 3 */
    .bss    : ...  { __bss_start = .; ...; __bss_end = .; }
}
```

Pontos-chaves:

1. **`.text` começa em `0x10000`**: é o que torna o `jmp 0x10000` do
   bootloader correto. `_start` (em `kernel/main.c`) é colocado logo ali.
2. **`.bss`** é a seção das variáveis globais não inicializadas (o que é
   `static ... array[8][8192]` sem valor inicial). Ela **não ocupa espaço no
   arquivo**, mas o linker define os símbolos `__bss_start`/`__bss_end` que o
   `kernel/init/bss.c` usa para **zerar a área** em tempo de execução
   (assim como o C runtime faz num SO normal — aqui somos nós que limpamos).
3. **`.user_text`**: o código do terminal (`user/user.c`) é compilado com um
   `__attribute__((section(".user_text")))` e entra nessa seção. Ele é
   colocado dentro do próprio `kernel.bin` (compartilha o mesmo arquivo e o
   mesmo espaço de endereços — sem paginação, é assim que o ring 3 executa
   código do kernel: é só um endereço a mais).
4. **`/DISCARD/`**: descarta metadados ELF que não interessam ao binário.

> Por que o kernel tem **que** saber o endereço? Porque sem paginação e sem
> relocação, o código foi compilado com endereços absolutos fixos. O linker
> script é o que "fixa" esses endereços.

## 3.4 Como o Makefile usa as seções

Pegue um exemplo: `kernel/init/bss.c` tem

```c
extern char __bss_start;
extern char __bss_end;
```

Essas são **variáveis que não existem no C**: são *símbolos do link*. O
linker script define `__bss_start = .` e `__bss_end = .`, e o C apenas toma
seus endereços com `&__bss_start`. É assim que o C conversa com o linker.

## 3.5 A imagem final (`os.img`)

```makefile
build/os.img: build/boot.bin build/kernel.bin
	cp build/boot.bin $@                      # setor 0 = bootloader
	truncate -s $(DISK_SIZE) $@               # estica até 1.44 MB
	dd if=build/kernel.bin of=$@ bs=512 seek=1 conv=notrunc   # kernel a partir do setor 1
```

```
setor 0  → boot.bin (512 bytes, o bootloader)
setor 1+ → kernel.bin (o kernel inteiro, até 32 KiB lidos pelo boot)
```

Isso casa perfeitamente com o bootloader: `KERNEL_SECTORS equ 64` e o DAP
com `dq 1` (LBA inicial = 1) — o bootloader lê os 64 setores a partir do
setor 1, que é onde o kernel foi gravado.

## 3.6 Regras de dependência

As regras genéricas do Makefile usam `patsubst` para transformar
`kernel/a/b.c` em `build/a/b.o`, criando a árvore `build/` automaticamente:

```makefile
build/%.o: kernel/%.c | build
	$(CC) $(CFLAGS) $< -o $@
```

O `| build` (order-only) garante que a pasta exista antes de compilar, sem
fazer todos os objetos dependerem do "conteúdo" da pasta.

## Próximo passo

O kernel está na memória em endereço certo. Agora ele precisa conversar com
a CPU pelos mecanismos modernos: GDT, IDT e PIC — `docs/04-gdt-idt-pic.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `Makefile` | todo o pipeline de build |
| `linker/kernel.ld` | endereços, seções, `__bss_start/__bss_end` |
| `kernel/init/bss.c` | quem zera o `.bss` em runtime |
| `kernel/main.c` | `.text.boot` + entrada `_start` |
| `user/user.c` linha `prog_terminal` | `.user_text` (ring 3) |