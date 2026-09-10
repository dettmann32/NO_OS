#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_TYPE_DIR   1
#define FS_TYPE_FILE  2

#define FS_NAME_MAX   24
#define FS_MAX_NODES  128
#define FS_MAX_DEPTH  16
#define FS_MAX_FILE   512

/* Inicializa o sistema de arquivos (raiz "/" + conteúdo de demonstração) */
int fs_init(void);

/* Procura um filho com o nome dado dentro do diretório `dir`.
 * Retorna o índice do nó ou -1. */
int fs_lookup(uint32_t dir, const char *name);

/* Cria diretório/arquivo vazio dentro de `dir`. Retorna o nó ou -1. */
int fs_mkdir(uint32_t dir, const char *name);
int fs_create_file(uint32_t dir, const char *name);

/* Escreve `len` bytes no arquivo (sobrescreve). Retorna bytes escritos. */
int fs_write_file(uint32_t node, const char *data, uint32_t len);

/* Remove um arquivo vazio ou diretório vazio do diretório pai. */
int fs_remove(uint32_t dir, const char *name);

/* Acessores de um nó */
uint32_t fs_get_type(uint32_t node);
uint32_t fs_get_size(uint32_t node);
uint32_t fs_get_parent(uint32_t node);
uint32_t fs_get_first_child(uint32_t node);
uint32_t fs_get_next_sibling(uint32_t node);
const char *fs_get_name(uint32_t node);
const char *fs_get_data(uint32_t node);

/* Monta o caminho completo (tipo "/a/b") de um nó. */
void fs_build_path(uint32_t node, char *out, uint32_t max);

#endif