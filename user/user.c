#include <stdint.h>
#include "../kernel/include/syscall.h"

/* ============================================================
 * TERMINAL (shell)
 * Programa chamado pelo kernel na inicialização. É uma tarefa
 * de ring 3 que só usa as syscalls; navega no sistema de
 * arquivos como o bash do Linux: ls, cd, mkdir, touch, cat,
 * echo >, rm, pwd, clear, help.
 * ============================================================ */

#define LINE_MAX 128
#define NAME_MAX 32

static char line[LINE_MAX];
static int line_n;

/* ---------- utilidades de string ---------- */

static int str_len(const char *s) {
    int n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int str_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void println(const char *s) {
    sys_write(s, str_len(s));
    sys_write("\n", 1);
}

/* ---------- leitura da linha de comando ---------- */

static void read_line(void) {
    line_n = 0;
    for (;;) {
        int c = sys_read();
        if (c < 0) {
            continue;
        }

        if (c == 0x1B) {                    /* Esc: limpa a linha */
            for (int i = 0; i < line_n; i++) {
                sys_write("\b \b", 3);
            }
            line_n = 0;
            continue;
        }

        if (c == '\n') {
            sys_write("\n", 1);
            line[line_n] = '\0';
            return;
        }

        if (c == '\b') {                    /* Backspace */
            if (line_n > 0) {
                sys_write("\b \b", 3);
                line_n--;
            }
            continue;
        }

        if (c == '\t') {
            continue;
        }

        if (line_n >= LINE_MAX - 1) {
            continue;
        }

        line[line_n++] = (char)c;
        char out = (char)c;
        sys_write(&out, 1);
    }
}

/* ---------- parser (tokens separados por espaço) ---------- */

static int n_args;
static char *args[8];

static void parse_line(void) {
    n_args = 0;
    int i = 0;
    while (line[i] != '\0' && n_args < 8) {
        while (line[i] == ' ') {
            i++;
        }
        if (line[i] == '\0') {
            break;
        }
        args[n_args++] = &line[i];
        while (line[i] != '\0' && line[i] != ' ') {
            i++;
        }
        if (line[i] != '\0') {
            line[i++] = '\0';
        }
    }
}

/* ---------- comandos ---------- */

static void cmd_help(void) {
    println("Comandos: ls [dir] | cd <dir> | mkdir <nome> | touch <arq>");
    println("          cat <arq> | echo <texto> > <arq> | rm <arq>");
    println("          pwd | clear | help | exit");
}

static void cmd_pwd(void) {
    char buf[256];
    sys_pwd(buf, sizeof(buf));
    println(buf);
}

static void cmd_ls(const char *dir) {
    int fd = sys_open((dir && dir[0] != '\0') ? dir : ".", SYS_OPEN_READ);
    if (fd < 0) {
        println("ls: nao encontrado");
        return;
    }
    char buf[92];
    for (;;) {
        long n = sys_fread(fd, buf, (int)sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';

        /* formata "nome:D:0\n" -> "nome/" e "nome:F:size\n" -> "nome  (size)" */
        int i = 0;
        while (i < n) {
            char nm[NAME_MAX];
            int k = 0;
            while (i < n && buf[i] != ':' && k < (int)sizeof(nm) - 1) {
                nm[k++] = buf[i++];
            }
            nm[k] = '\0';
            if (i >= n) {
                break;
            }
            char t = buf[i + 1];            /* 'D' ou 'F' */
            i += 2;
            /* pula o tamanho até o '\n' */
            while (i < n && buf[i] != '\n') {
                i++;
            }
            i++;

            if (t == 'D') {
                sys_write(nm, str_len(nm));
                sys_write("/\n", 2);
            } else {
                sys_write(nm, str_len(nm));
                sys_write("\n", 1);
            }
        }
    }
    sys_close(fd);
}

static void cmd_cd(const char *dir) {
    if (sys_chdir(dir) < 0) {
        println("cd: diretorio inexistente");
    }
}

static void cmd_mkdir(const char *name) {
    if (sys_mkdir(name) < 0) {
        println("mkdir: falhou (ja existe?)");
    }
}

static void cmd_touch(const char *name) {
    int fd = sys_open(name, SYS_OPEN_CREATE);
    if (fd < 0) {
        println("touch: falhou");
        return;
    }
    sys_close(fd);
}

static void cmd_rm(const char *name) {
    if (sys_rm(name) < 0) {
        println("rm: falhou (nao vazio/inexistente)");
    }
}

static void cmd_cat(const char *name) {
    int fd = sys_open(name, SYS_OPEN_READ);
    if (fd < 0) {
        println("cat: arquivo inexistente");
        return;
    }
    char buf[257];
    long total = 0;
    for (;;) {
        long n = sys_fread(fd, buf, 256);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        sys_write(buf, (int)n);
        total += n;
    }
    sys_close(fd);
    if (total > 0) {
        sys_write("\n", 1);
    }
}

static void cmd_echo(int argc, char **argv) {
    if (argc >= 4 && str_eq(argv[2], ">")) {
        int fd = sys_open(argv[3], SYS_OPEN_WRITE);
        if (fd < 0) {
            println("echo: falhou");
            return;
        }
        sys_fwrite(fd, argv[1], str_len(argv[1]));
        sys_close(fd);
    } else {
        println(argv[1]);
    }
}

/* ---------- prompt e main ---------- */

static void print_prompt(void) {
    char buf[256];
    sys_pwd(buf, sizeof(buf));
    sys_write(buf, str_len(buf));
    sys_write("$ ", 2);
}

__attribute__((section(".user_text"), used))
void prog_terminal(void) {
    println("Kernel Bare-Metal x86_64 - Terminal");
    println("Digite 'help' para a lista de comandos.");

    for (;;) {
        print_prompt();
        read_line();

        if (line_n == 0) {
            continue;
        }

        parse_line();
        const char *cmd = args[0];

        if (str_eq(cmd, "help")) {
            cmd_help();
        } else if (str_eq(cmd, "pwd")) {
            cmd_pwd();
        } else if (str_eq(cmd, "ls")) {
            cmd_ls(n_args > 1 ? args[1] : "");
        } else if (str_eq(cmd, "cd")) {
            if (n_args < 2) {
                sys_chdir("/");
            } else {
                cmd_cd(args[1]);
            }
        } else if (str_eq(cmd, "mkdir")) {
            if (n_args < 2) {
                println("uso: mkdir <nome>");
            } else {
                cmd_mkdir(args[1]);
            }
        } else if (str_eq(cmd, "touch")) {
            if (n_args < 2) {
                println("uso: touch <arquivo>");
            } else {
                cmd_touch(args[1]);
            }
        } else if (str_eq(cmd, "cat")) {
            if (n_args < 2) {
                println("uso: cat <arquivo>");
            } else {
                cmd_cat(args[1]);
            }
        } else if (str_eq(cmd, "echo")) {
            if (n_args < 2) {
                println("");
            } else {
                cmd_echo(n_args, args);
            }
        } else if (str_eq(cmd, "rm")) {
            if (n_args < 2) {
                println("uso: rm <arquivo>");
            } else {
                cmd_rm(args[1]);
            }
        } else if (str_eq(cmd, "clear")) {
            for (int i = 0; i < 24; i++) {
                println("");
            }
        } else if (str_eq(cmd, "exit")) {
            println("encerrando terminal");
            sys_exit();
        } else {
            println("comando desconhecido (help)");
        }
    }
}