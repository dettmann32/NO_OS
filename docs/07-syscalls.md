# 07 — Syscalls: a porta do ring 3 para o kernel

> Arquivos: `kernel/syscall/syscall.c`, `kernel/include/syscall.h`,
> `kernel/arch/x86/idt_stubs.asm`

O terminal roda em **anel 3**. Ele não pode (e não deve) escrever na tela
diretamente. Tudo que ele precisa (escrever, ler teclado, mexer em arquivos)
passa por **syscalls** — instruções `int 0x80` que pedem ao kernel para
fazer algo e devolver o resultado. É o mesmo mecanismo do Linux/Windows,
só que nós mesmos implementamos.

## 7.1 A ideia em uma linha

1. Usuário coloca **número da syscall** em `eax` e argumentos em `ebx/ecx/edx`.
2. Usuário executa **`int 0x80`** (uma interrupção de software).
3. A CPU salva o contexto, troca para anel 0 (via TSS), consulta a IDT[0x80].
4. O kernel lê os registradores, executa a função desejada e grava o
   resultado **em eax**.
5. `iret` volta para o usuário no exato ponto seguinte ao `int`.

## 7.2 Como a IDT permite isso: gate com DPL 3

Em `kernel/syscall/syscall.c`:

```c
void syscall_init(void) {
    idt_set_gate(0x80, (uint32_t)syscall80, 0x08, 0xEE);
}
```

- vetor `0x80` → aponta para `syscall80` (assembly);
- seletor `0x08` (kernel code);
- flags **`0xEE`** = interrupt gate com **DPL 3** → o ring 3 pode disparar.
  (Se fosse `0x8E`, o anel 3 levaria General Protection Fault ao tentar.)

## 7.3 O stub do `int 0x80` (assembly)

```asm
syscall80:
    push dword 0          ; err_code fictício
    push dword 0x80       ; int_no (para saber em registers_t)
    jmp syscall_common_stub

syscall_common_stub:
    pusha                 ; salva EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX
    push esp              ; ponteiro para registers_t
    call syscall_handler
    add esp, 4            ; remove ponteiro
    popa                  ; restaura registradores (incl. eax com o resultado)
    add esp, 8            ; descarta int_no/err_code
    iret                  ; volta para o usuário
```

> Note: o código da syscall usa **a mesma pilha de kernel da tarefa** e o
> mesmo formato `registers_t`. É por isso que o `tss.esp0` precisa estar
> sempre certo (o escalonador o atualiza a cada troca).

## 7.4 O dispatcher em C

A partir do `registers_t`, o único trabalho do C é:

```c
void syscall_handler(registers_t *r) {
    task_t *cur = task_current();
    switch (r->eax) {            // números das syscalls (syscall.h)
        case SYS_WRITE:  do_write((char*)r->ebx, r->ecx); r->eax = 0; break;
        case SYS_READ:   r->eax = keyboard_read();        break;
        case SYS_EXIT:   scheduler_exit_current();        break;
        case SYS_GET_TICK: r->eax = pit_get_ticks();      break;
        case SYS_FILE_OPEN: r->eax = fs_open_sys(...);    break;
        // ... todas as syscalls de arquivo
        default: r->eax = -1;                              // "não existe"
    }
}
```

Repare: o resultado da syscall é **gravado em `r->eax`**. Como o assembly
faz `popa` ao final, o `eax` restaurado no registrador **é** esse valor — é
assim que o C "devolve" dados ao assembly de volta para o usuário.

## 7.5 O lado do usuário: `syscall.h` (inline asm)

O C do usuário não escreve `int 0x80` na mão: ele usa wrappers `static inline`
que já fazem o trabalho. Exemplo:

```c
static inline long sys_write(const char *str, int len) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)              // saida: eax após a syscall
                     : "a"(SYS_WRITE),        // entrada: eax = número
                       "b"(str),              //         ebx = buffer
                       "c"(len)               //         ecx = tamanho
                     : "memory");
    return ret;
}
```

A convenção ficou assim (documentada em `syscall.h`):

| Registrador | Conteúdo na entrada |
|-------------|---------------------|
| `eax` | número da syscall |
| `ebx` | 1º argumento (ponteiro ou int) |
| `ecx` | 2º argumento |
| `edx` | 3º argumento |
| `eax` (saída) | valor de retorno |

Isso é um ABI interna: kernel e usuário precisam concordar nos números
(`SYS_WRITE=0 ... SYS_FS_RM=11`).

## 7.6 As syscalls existentes

| # | Nome | Faz |
|---|------|-----|
| 0 | `SYS_WRITE` | escreve `len` chars na tela (anel 0) |
| 1 | `SYS_READ` | lê 1 tecla da fila do teclado (ou -1 se vazia) |
| 2 | `SYS_EXIT` | marca a tarefa atual como `TASK_DONE` |
| 3 | `SYS_GET_TICK` | contador de ticks do PIT |
| 4 | `SYS_FILE_OPEN` | abre arquivo/dir (`SYS_OPEN_READ/CREATE/WRITE`) |
| 5 | `SYS_FILE_READ` | lê do fd (diretório → "linha", arquivo → bytes) |
| 6 | `SYS_FILE_WRITE` | grava no fd |
| 7 | `SYS_FILE_CLOSE` | libera o fd |
| 8 | `SYS_FS_MKDIR` | cria diretório no cwd |
| 9 | `SYS_FS_CHDIR` | muda o cwd da tarefa (`.` `..`) |
| 10 | `SYS_FS_GETCWD` | escreve o caminho atual no buffer |
| 11 | `SYS_FS_RM` | remove arquivo vazio |

Os modos de abertura (`SYS_OPEN_*`) significam:

- `SYS_OPEN_READ (0)` — abre existente para leitura;
- `SYS_OPEN_CREATE (1)` — cria se não existir (o `touch`);
- `SYS_OPEN_WRITE (2)` — trunca/cria e abre para escrita (o `echo >`).

## 7.7 O que NÃO passa por syscall?

- A tarefa ociosa (idle) roda em anel 0.
- O terminal mostra texto via `sys_write`; o kernel mostra via
  `console_putchar` direto — lado a lado no mesmo console.

## Próximo passo

As syscalls de arquivo usam o **sistema de arquivos em RAM** — o próximo
doc: `docs/08-filesystem.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `kernel/syscall/syscall.c` | dispatcher + implementação |
| `kernel/include/syscall.h` | números, modos, wrappers inline |
| `kernel/arch/x86/idt_stubs.asm` | `syscall80`/`syscall_common_stub` |
| `kernel/arch/x86/idt.c` | `idt_set_gate` (DPL) |
| `user/user.c` | quem chama as syscalls (o terminal) |