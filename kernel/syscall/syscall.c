#include <stdint.h>
#include "../include/syscall.h"
#include "../include/registers.h"
#include "../include/idt.h"
#include "../include/console.h"
#include "../include/keyboard.h"
#include "../include/pit.h"
#include "../include/task.h"
#include "../include/fs.h"

extern void syscall80(void);

/* Tabela de descritores de arquivo (um diretório/arquivo aberto) */
#define MAX_FDS 16
typedef struct {
    int used;
    uint32_t node;
    uint32_t pos;       /* offest de leitura; diretório: quantas linhas já lidas */
    int writable;
} filenode_t;

static filenode_t fds[MAX_FDS];

static void do_write(const char *str, long len) {
    for (long i = 0; i < len && str[i] != '\0'; i++) {
        console_putchar(str[i]);
    }
}

/* Serializa em uma linha a enésima entrada de um diretório:
 * "nome:D:0\n" (diretório) ou "nome:F:size\n" (arquivo). */
static int dir_line(uint32_t dir, uint32_t idx, char *out, uint32_t max) {
    uint32_t c = fs_get_first_child(dir);
    while (c != 0 && idx > 0) {
        c = fs_get_next_sibling(c);
        idx--;
    }
    if (c == 0) {
        return 0;
    }

    const char *name = fs_get_name(c);
    int o = 0;
    while (*name && o < (int)max - 1) {
        out[o++] = *name++;
    }
    out[o++] = ':';
    out[o++] = (fs_get_type(c) == FS_TYPE_DIR) ? 'D' : 'F';
    out[o++] = ':';

    uint32_t size = (fs_get_type(c) == FS_TYPE_DIR) ? 0 : fs_get_size(c);
    char num[12];
    int d = 0;
    do {
        num[d++] = (char)('0' + (size % 10));
        size /= 10;
    } while (size > 0);
    while (d > 0 && o < (int)max - 2) {
        out[o++] = num[--d];
    }
    out[o++] = '\n';
    out[o] = '\0';
    return o;
}

/* Resolve "." ".." "/" ou nome simples dentro do cwd */
static int resolve_dir(task_t *cur, const char *name, uint32_t *node) {
    if (name[0] == '\0') {
        *node = 0;
        return 0;
    }
    if (name[0] == '/') {
        *node = 0;
        return 0;
    }
    if (name[0] == '.' && name[1] == '\0') {
        *node = cur->cwd;
        return 0;
    }
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        uint32_t p = fs_get_parent(cur->cwd);
        *node = (cur->cwd == 0) ? 0 : p;
        return 0;
    }
    int n = fs_lookup(cur->cwd, name);
    if (n < 0) {
        return -1;
    }
    *node = (uint32_t)n;
    return 0;
}

static int fs_open_sys(task_t *cur, const char *name, int mode) {
    int i;
    for (i = 0; i < MAX_FDS; i++) {
        if (!fds[i].used) {
            break;
        }
    }
    if (i == MAX_FDS) {
        return -1;
    }

    if (mode != SYS_OPEN_READ &&
        (name[0] == '/' ||
         (name[0] == '.' && name[1] == '\0') ||
         (name[0] == '.' && name[1] == '.' && name[2] == '\0'))) {
        return -1;
    }

    uint32_t node = 0;
    if (mode == SYS_OPEN_READ) {
        if (resolve_dir(cur, name, &node) < 0) {
            return -1;
        }
    } else if (mode == SYS_OPEN_CREATE) {
        if (resolve_dir(cur, name, &node) < 0) {
            int d = fs_lookup(cur->cwd, name);
            if (d >= 0) {
                node = (uint32_t)d;
            } else {
                node = (uint32_t)fs_create_file(cur->cwd, name);
                if (node == 0 || node >= FS_MAX_NODES) {
                    return -1;
                }
            }
        }
    } else { /* SYS_OPEN_WRITE: trunca ou cria */
        if (resolve_dir(cur, name, &node) == 0 && fs_get_type(node) == FS_TYPE_DIR) {
            return -1;
        }
        if (node == 0 || fs_get_type(node) != FS_TYPE_FILE) {
            int d = fs_lookup(cur->cwd, name);
            if (d >= 0) {
                node = (uint32_t)d;
            } else {
                node = (uint32_t)fs_create_file(cur->cwd, name);
                if (node == 0 || node >= FS_MAX_NODES) {
                    return -1;
                }
            }
        }
        fs_write_file(node, "", 0);   /* trunca */
    }

    fds[i].used = 1;
    fds[i].node = node;
    fds[i].pos = 0;
    fds[i].writable = ((mode == SYS_OPEN_CREATE) || (mode == SYS_OPEN_WRITE)) &&
                      (fs_get_type(node) == FS_TYPE_FILE);
    return i;
}

static long fs_read_sys(int fd, char *buf, int len) {
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].used) {
        return -1;
    }
    uint32_t node = fds[fd].node;

    if (fs_get_type(node) == FS_TYPE_DIR) {
        char line[FS_NAME_MAX + 12];
        int n = dir_line(node, fds[fd].pos, line, sizeof(line));
        if (n == 0) {
            return 0;
        }
        fds[fd].pos++;
        if (len > n) {
            len = n;
        }
        for (int k = 0; k < n; k++) {
            buf[k] = line[k];
        }
        return len;
    }

    uint32_t size = fs_get_size(node);
    if (fds[fd].pos >= size) {
        return 0;
    }
    long n = ((uint32_t)len > size - fds[fd].pos) ? (long)(size - fds[fd].pos) : len;
    const char *data = fs_get_data(node);
    for (long k = 0; k < n; k++) {
        buf[k] = data[fds[fd].pos + (uint32_t)k];
    }
    fds[fd].pos += (uint32_t)n;
    return n;
}

static long fs_write_sys(int fd, const char *buf, int len) {
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].used || !fds[fd].writable) {
        return -1;
    }
    int w = fs_write_file(fds[fd].node, buf, (uint32_t)len);
    if (w >= 0) {
        fds[fd].pos = (uint32_t)w;
    }
    return w;
}

void syscall_handler(registers_t *r) {
    task_t *cur = task_current();

    switch (r->eax) {
        case SYS_WRITE:
            if (r->ebx != 0 && r->ecx > 0) {
                do_write((const char *)r->ebx, r->ecx);
            }
            r->eax = 0;
            break;

        case SYS_READ:
            r->eax = keyboard_read();
            break;

        case SYS_EXIT:
            scheduler_exit_current();
            r->eax = 0;
            break;

        case SYS_GET_TICK:
            r->eax = pit_get_ticks();
            break;

        case SYS_FILE_OPEN:
            r->eax = fs_open_sys(cur, (const char *)r->ebx, (int)r->ecx);
            break;

        case SYS_FILE_READ:
            r->eax = fs_read_sys((int)r->ebx, (char *)r->ecx, (int)r->edx);
            break;

        case SYS_FILE_WRITE:
            r->eax = fs_write_sys((int)r->ebx, (const char *)r->ecx, (int)r->edx);
            break;

        case SYS_FILE_CLOSE:
            if ((long)r->ebx >= 0 && (long)r->ebx < MAX_FDS) {
                fds[r->ebx].used = 0;
            }
            r->eax = 0;
            break;

        case SYS_FS_MKDIR:
            r->eax = fs_mkdir(cur->cwd, (const char *)r->ebx);
            break;

        case SYS_FS_CHDIR:
            if (cur) {
                uint32_t node = 0;
                if (resolve_dir(cur, (const char *)r->ebx, &node) == 0 &&
                    fs_get_type(node) == FS_TYPE_DIR) {
                    cur->cwd = node;
                    r->eax = 0;
                } else {
                    r->eax = -1;
                }
            } else {
                r->eax = -1;
            }
            break;

        case SYS_FS_GETCWD: {
            char *out = (char *)r->ebx;
            if (cur && out) {
                fs_build_path(cur->cwd, out, (uint32_t)r->ecx);
                r->eax = 0;
            } else {
                r->eax = -1;
            }
            break;
        }

        case SYS_FS_RM:
            r->eax = fs_remove(cur->cwd, (const char *)r->ebx);
            break;

        default:
            r->eax = -1;
            break;
    }
}

void syscall_init(void) {
    /* Gate de interrupção com DPL 3: pode ser chamado do ring 3 via int 0x80 */
    idt_set_gate(0x80, (uint32_t)syscall80, 0x08, 0xEE);
}