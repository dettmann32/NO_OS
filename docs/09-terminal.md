# 09 — O terminal (shell) no ring 3

> Arquivo: `user/user.c`

Este é o "programa de usuário": um **shell** estilo bash que navega no
sistema de arquivos. Ele roda como tarefa de ring 3 e o único jeito de
fazer qualquer coisa é pelas **syscalls**. Ou seja: é o exemplo perfeito
de "programa normal" usando um "SO".

## 9.1 Como ele vira uma tarefa

Em `kernel/main.c`:

```c
task_create(prog_terminal, 1);   // is_user = 1 → roda no anel 3
scheduler_add_idle();            // + tarefa ociosa
scheduler_begin();
```

O `prog_terminal` é a função abaixo:

```c
__attribute__((section(".user_text"), used))
void prog_terminal(void) { ... }
```

Ela vive na seção `.user_text` do próprio kernel (ver `docs/03-build.md`) —
sem paginação, o endereço é só um número; o anel 3 é que restringe o poder.

## 9.2 Main loop do shell

```c
void prog_terminal(void) {
    println("Kernel Bare-Metal x86_64 - Terminal");
    println("Digite 'help' para a lista de comandos.");

    for (;;) {
        print_prompt();      // imprime "/docs$ " (via sys_pwd)
        read_line();         // lê a linha de comando tecla a tecla
        parse_line();        // quebra em tokens (argv)
        if (str_eq(args[0], "help"))   cmd_help();
        else if (...) // comando correspondente
        // ...
    }
}
```

> Perceba: é um loop infinito. O terminal nunca "termina" — quando o
> usuário digita `exit`, ele chama `sys_exit()`, que marca a tarefa como
> `TASK_DONE`. O escalonador para de escalá-lo e o idle assume a CPU.

## 9.3 Leitura de linha: `read_line()`

Lê tecla a tecla via `sys_read()` (que vem da fila do teclado — ver
`docs/05-drivers.md`):

- `\n` → quebra linha e devolve;
- `Esc` (0x1B) → limpa a linha inteira (escreve `\b \b` por caractere);
- `\b` → apaga um caractere (mesmo truque);
- `\t` → ignora;
- senão, guarda o caractere no buffer `line[]` e **ecoa** (escreve na tela).
- se a linha enche (`LINE_MAX-1`), ignora.

O truque `"\b \b"` é: backspace, espaço, backspace — apaga o caractere
anterior da tela do console.

## 9.4 Parser: `parse_line()`

Quebra `line[]` em até 8 tokens separados por espaço, colocando `\0` no
lugar dos espaços e guardando ponteiros:

```c
while (line[i] != '\0' && n_args < 8) {
    while (line[i] == ' ') i++;        // pula espaços
    args[n_args++] = &line[i];         // início do token
    while (line[i] != '\0' && line[i] != ' ') i++;
    if (line[i] != '\0') line[i++] = '\0';   // corta na borda
}
```

Ex.: `echo oi > x.txt` → `args = {"echo", "oi", ">", "x.txt"}`,
`n_args = 4`.

## 9.5 Comandos – o mesmo padrão, syscalls

| Comando | Código | Como funciona |
|---------|--------|----------------|
| `ls [dir]` | `cmd_ls` | abre o dir, lê linha a linha via `dir_line`, mostra `nome/` para dir e `nome` para arquivo |
| `cd <dir>` | `cmd_cd` | `sys_chdir` (suporta `.` `..` `/` via `resolve_dir`) |
| `mkdir` | `cmd_mkdir` | `sys_mkdir` no cwd |
| `touch` | `cmd_touch` | `sys_open(name, SYS_OPEN_CREATE)` + `sys_close` |
| `echo text > arquivo` | `cmd_echo` | detecta o token `>` em `argv[2]` e abre com `SYS_OPEN_WRITE` |
| `echo text` | `cmd_echo` | imprime direto |
| `cat <arq>` | `cmd_cat` | loop `sys_fread` + `sys_write` |
| `rm <arq>` | `cmd_rm` | `sys_rm` |
| `pwd` | `cmd_pwd` | `sys_pwd` |
| `clear` | no main | imprime 24 quebras de linha |
| `help` | `cmd_help` | lista os comandos |
| `exit` | no main | `sys_exit()` |

### `ls` com fds — um exemplo completo

```c
static void cmd_ls(const char *dir) {
    int fd = sys_open(dir ? dir : ".", SYS_OPEN_READ);   // "" → "."
    char buf[92];
    for (;;) {
        long n = sys_fread(fd, buf, sizeof(buf)-1);       // uma "linha"
        if (n <= 0) break;                                 // fim do dir
        // buf tem "nome:D:0\n" ou "nome:F:52\n"
        ... separa nome/tipo/tamanho e imprime "nome/" ou "nome"
    }
    sys_close(fd);
}
```

### `cat` — padrão de leitura de arquivo

```c
for (;;) {
    long n = sys_fread(fd, buf, 256);   // lê até 256 bytes
    if (n <= 0) break;                   // fim do arquivo
    sys_write(buf, (int)n);              // despeja na tela
}
```

## 9.6 Código de usuário é "seguro" por construção?

O anel 3 não pode tocar hardware diretamente — se tentar `in`/`out`
levará General Protection Fault. Ele só conversa com o mundo por syscalls.
Porém este projeto não tem **páginas de memória por tarefa**: todos os
processos compartilham o mesmo espaço de endereço (incluindo o kernel!).
O isolamento "real" (cada processo com sua própria tabela de páginas) é
uma evolução futura — mas o mecanismo de syscall já está aqui e o anel
já está funcional.

## Nota: por que não um parser em C++ ou com regex...

Em bare-metal, sem libc, tudo precisa ser implementado à mão — por isso
`str_len`/`str_eq` e o parsing simples de espaço. Não existe `string.h`.

## Próximo passo

Viu as peças separadas. O doc final amarra tudo numa **linha do tempo
completa do boot até o prompt**: `docs/10-fluxo.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `user/user.c` | o programa inteiro (terminal) |
| `kernel/include/syscall.h` | wrappers usados pelo terminal |
| `kernel/main.c` | `task_create(prog_terminal, 1)` |