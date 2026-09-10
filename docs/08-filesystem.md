# 08 — Sistema de arquivos em RAM

> Arquivo: `kernel/fs/fs.c`, `kernel/include/fs.h`

Este kernel não tem disco real: o "disco" é uma **área da memória do
kernel**. O sistema de arquivos vive num **array de nós** em RAM. É simples
e perfeito para entender os conceitos de diretório, arquivo, pai/filho,
caminho — sem a complexidade de disco/formatação.

## 8.1 O "disco" = um array de nós

```c
#define FS_MAX_NODES 128   // cabe 128 arquivos/diretórios

typedef struct {
    uint32_t type;          // 0 = livre, 1 = diretório, 2 = arquivo
    char name[FS_NAME_MAX]; // nome (até 24 chars)
    uint32_t parent;        // nó pai (para montar caminhos)
    uint32_t first_child;   // dir: primeiro filho
    uint32_t next_sibling;  // irmão seguinte (lista do pai)
    uint32_t size;          // arquivo: bytes usados
    char data[FS_MAX_FILE]; // arquivo: o conteúdo (máx 508 bytes)
} fs_node_t;

static fs_node_t nodes[FS_MAX_NODES];   // ⭐ o "disco" inteiro
```

Cada diretório tem uma **lista encadeada** de filhos:

```
nodes[0] "/" (diretório)
   first_child ──► nodes[1] "docs" (dir)
                      │  next_sibling ──► nodes[2] "usr" (dir)
                      ▼                        │ next_sibling...
                  (filhos de docs...)
```

## 8.2 As operações básicas

### Procurar filho (`fs_lookup`)

```c
int fs_lookup(uint32_t dir, const char *name) {
    uint32_t c = nodes[dir].first_child;
    while (c) {
        if (name_eq(nodes[c].name, name) != 0)
            return (int)c;          // achou: devolve o índice do nó
        c = nodes[c].next_sibling;   // senão, anda na lista
    }
    return -1;                       // não existe
}
```

### Criar nós (`fs_alloc` + `fs_link_child`)

1. `fs_alloc` acha um slot com `type == 0` (livre) e preenche;
2. `fs_link_child` adiciona o novo nó no **fim** da lista de filhos do pai
   (anda até o último e liga `next_sibling`).

### Escrever arquivo (`fs_write_file`)

Copia bytes para `nodes[node].data` e atualiza `size`:

```c
int fs_write_file(uint32_t node, const char *data, uint32_t len) {
    if (len > FS_MAX_FILE) len = FS_MAX_FILE;   // proteção
    for (uint32_t i = 0; i < len; i++)
        nodes[node].data[i] = data[i];
    nodes[node].size = len;
    return (int)len;
}
```

### Remover (`fs_remove`)

Localiza o filho na lista; só permite remover **arquivo vazio ou diretório
vazio** (diretório com filhos → erro). Desliga o nó da lista e marca
`type = 0` (libera o slot).

## 8.3 `fs_build_path` — montar `/docs/projetos` a partir de um nó

O `pwd` do terminal precisa do caminho completo. A função sobe pela cadeia
de `parent` guardando os segmentos, depois reconstrói na ordem:

```c
/* sobe: nodes[projetos].parent → "docs" → "/" */
segs[0]="projetos", segs[1]="docs";  cur=0 para
/* monta de trás para frente: "/docs/projetos" */
```

## 8.4 A leitura de diretórios COM fds: método "posição"

Como syscalls precisam de um **fd** (`SYS_FILE_OPEN/FILE_READ/FILE_CLOSE`),
ler um diretório usa a mesma "API" de arquivo. A diferença: para diretório,
cada leitura devolve **uma linha** com o formato

```
nome:T:tamanho\n     (T = D para diretório, F para arquivo)
```

A função `dir_line` (em `kernel/syscall/syscall.c`) põe isso no buffer; o
`fds[fd].pos` guarda **quantas linhas já foram lidas** (funciona como um
"cursor"). Quando acabam as entradas, devolve 0.

Exemplo de `ls` na raiz:

```
read #1 → "docs:D:0\n"
read #2 → "usr:D:0\n"
read #3 → "inicio.txt:F:52\n"
read #4 → "" (fim)
```

O terminal transforma isso em `docs/`, `usr/`, `inicio.txt`.

## 8.5 `fs_init` — o conteúdo que nasce junto com o kernel

No boot, o kernel cria uma árvore de demonstração:

```
/            (raiz)
├── docs/        diretório
│   ├── leiame.txt   → "Bem-vindo ao nosso kernel!"
│   └── projetos/    diretório
├── usr/          diretório
│   └── bin       arquivo vazio
└── inicio.txt    → "terminal pronto: ls, cd, mkdir, touch, echo, cat, rm"
```

Isso mostra o diretório já existindo quando o terminal abre.

## 8.6 Limites de projeto

| Constante | Valor | O que limita |
|-----------|-------|--------------|
| `FS_MAX_NODES` | 128 | total de arquivos/dirs |
| `FS_NAME_MAX` | 24 | tamanho do nome |
| `FS_MAX_FILE` | 512 | tamanho de um arquivo |
| `FS_MAX_DEPTH` | 16 | profundidade de caminho (`/a/b/c/.../`) |

Como o "disco" é estático (`nodes[]` é global), não há alocação dinâmica —
só um array pré-reservado.

## Próximo passo

O sistema de arquivos é o "backend" do terminal. O próximo doc mostra o
shell (front-end no ring 3): `docs/09-terminal.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `kernel/fs/fs.c` | implementação do FS (nós, operações, `fs_init`) |
| `kernel/include/fs.h` | constantes e API do FS |
| `kernel/syscall/syscall.c` | `dir_line`, `resolve_dir`, fds (proto de fd) |