#include <stdint.h>
#include "../include/fs.h"

/* Sistema de arquivos simples em memória (RAM disk):
 * - nó 0 é a raiz "/" (diretório);
 * - cada diretório mantém uma lista encadeada (first_child/next_sibling)
 *   do seus filhos;
 * - cada arquivo guarda o conteúdo em `data` (até FS_MAX_FILE bytes).
 */

typedef struct {
    uint32_t type;          /* FS_TYPE_DIR ou FS_TYPE_FILE (0 = livre) */
    char name[FS_NAME_MAX];
    uint32_t parent;
    uint32_t first_child;   /* dir: primeiro filho */
    uint32_t next_sibling;  /* lista de irmãos dentro do pai */
    uint32_t size;          /* arquivo: tamanho em bytes */
    char data[FS_MAX_FILE];
} fs_node_t;

static fs_node_t nodes[FS_MAX_NODES];

static uint32_t node_parent(uint32_t n) {
    return nodes[n].parent;
}

static int name_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Procura um slot livre e marca como usado. */
static int fs_alloc(uint32_t type, const char *name, uint32_t parent) {
    for (uint32_t i = 1; i < FS_MAX_NODES; i++) {
        if (nodes[i].type == 0) {
            nodes[i].type = type;
            for (uint32_t j = 0; j < FS_NAME_MAX; j++) {
                nodes[i].name[j] = name[j];
                if (name[j] == '\0') {
                    break;
                }
            }
            nodes[i].parent = parent;
            nodes[i].first_child = 0;
            nodes[i].next_sibling = 0;
            nodes[i].size = 0;
            return (int)i;
        }
    }
    return -1;
}

int fs_lookup(uint32_t dir, const char *name) {
    if (dir >= FS_MAX_NODES || nodes[dir].type != FS_TYPE_DIR) {
        return -1;
    }
    uint32_t c = nodes[dir].first_child;
    while (c) {
        if (name_eq(nodes[c].name, name) != 0) {
            return (int)c;
        }
        c = nodes[c].next_sibling;
    }
    return -1;
}

static void fs_link_child(uint32_t dir, uint32_t child) {
    uint32_t last = nodes[dir].first_child;
    if (last == 0) {
        nodes[dir].first_child = child;
        return;
    }
    while (nodes[last].next_sibling) {
        last = nodes[last].next_sibling;
    }
    nodes[last].next_sibling = child;
}

int fs_mkdir(uint32_t dir, const char *name) {
    if (name[0] == '\0' || dir >= FS_MAX_NODES || nodes[dir].type != FS_TYPE_DIR) {
        return -1;
    }
    if (fs_lookup(dir, name) >= 0) {
        return -1;
    }
    int n = fs_alloc(FS_TYPE_DIR, name, dir);
    if (n < 0) {
        return -1;
    }
    fs_link_child(dir, (uint32_t)n);
    return n;
}

int fs_create_file(uint32_t dir, const char *name) {
    if (name[0] == '\0' || dir >= FS_MAX_NODES || nodes[dir].type != FS_TYPE_DIR) {
        return -1;
    }
    if (fs_lookup(dir, name) >= 0) {
        return -1;
    }
    int n = fs_alloc(FS_TYPE_FILE, name, dir);
    if (n < 0) {
        return -1;
    }
    fs_link_child(dir, (uint32_t)n);
    return n;
}

int fs_write_file(uint32_t node, const char *data, uint32_t len) {
    if (node >= FS_MAX_NODES || nodes[node].type != FS_TYPE_FILE) {
        return -1;
    }
    if (len > FS_MAX_FILE) {
        len = FS_MAX_FILE;
    }
    for (uint32_t i = 0; i < len; i++) {
        nodes[node].data[i] = data[i];
    }
    nodes[node].size = len;
    return (int)len;
}

int fs_remove(uint32_t dir, const char *name) {
    if (dir >= FS_MAX_NODES || nodes[dir].type != FS_TYPE_DIR) {
        return -1;
    }
    uint32_t p = 0;
    uint32_t c = nodes[dir].first_child;
    while (c) {
        if (name_eq(nodes[c].name, name) != 0) {
            if (nodes[c].type == FS_TYPE_DIR && nodes[c].first_child != 0) {
                return -1;          /* diretório não vazio */
            }
            if (p == 0) {
                nodes[dir].first_child = nodes[c].next_sibling;
            } else {
                nodes[p].next_sibling = nodes[c].next_sibling;
            }
            nodes[c].type = 0;      /* libera o slot */
            return 0;
        }
        p = c;
        c = nodes[c].next_sibling;
    }
    return -1;
}

uint32_t fs_get_type(uint32_t node) {
    return nodes[node].type;
}
uint32_t fs_get_size(uint32_t node) {
    return nodes[node].size;
}
uint32_t fs_get_parent(uint32_t node) {
    return node_parent(node);
}
uint32_t fs_get_first_child(uint32_t node) {
    return nodes[node].first_child;
}
uint32_t fs_get_next_sibling(uint32_t node) {
    return nodes[node].next_sibling;
}
const char *fs_get_name(uint32_t node) {
    return nodes[node].name;
}
const char *fs_get_data(uint32_t node) {
    return nodes[node].data;
}

void fs_build_path(uint32_t node, char *out, uint32_t max) {
    if (node == 0) {
        out[0] = '/';
        if (max > 1) {
            out[1] = '\0';
        }
        return;
    }

    char segs[FS_MAX_DEPTH][FS_NAME_MAX];
    int n = 0;
    uint32_t cur = node;
    while (cur != 0 && n < FS_MAX_DEPTH) {
        const char *nm = nodes[cur].name;
        uint32_t i = 0;
        while (nm[i] && i < FS_NAME_MAX - 1) {
            segs[n][i] = nm[i];
            i++;
        }
        segs[n][i] = '\0';
        n++;
        cur = nodes[cur].parent;
    }

    uint32_t o = 0;
    for (int k = n - 1; k >= 0 && o < max - 1; k--) {
        out[o++] = '/';
        uint32_t i = 0;
        while (segs[k][i] && o < max - 1) {
            out[o++] = segs[k][i++];
        }
    }
    if (o == 0) {
        out[o++] = '/';
    }
    out[o] = '\0';
}

/* Cria o conteúdo inicial de demonstração do sistema de arquivos */
int fs_init(void) {
    for (uint32_t i = 0; i < FS_MAX_NODES; i++) {
        nodes[i].type = 0;
    }

    /* raiz "/" */
    nodes[0].type = FS_TYPE_DIR;
    nodes[0].name[0] = '/';
    nodes[0].name[1] = '\0';
    nodes[0].parent = 0;
    nodes[0].first_child = 0;
    nodes[0].next_sibling = 0;

    int docs = fs_mkdir(0, "docs");
    int src = fs_mkdir(0, "usr");
    if (docs > 0) {
        int leia = fs_create_file((uint32_t)docs, "leiame.txt");
        if (leia > 0) {
            fs_write_file((uint32_t)leia, "Bem-vindo ao nosso kernel!", 26);
        }
        fs_mkdir((uint32_t)docs, "projetos");
    }
    if (src > 0) {
        fs_create_file((uint32_t)src, "bin");
    }

    int inicio = fs_create_file(0, "inicio.txt");
    if (inicio > 0) {
        fs_write_file((uint32_t)inicio, "terminal pronto: ls, cd, mkdir, touch, echo, cat, rm", 52);
    }

    return 0;
}