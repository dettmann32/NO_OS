#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE        0
#define SYS_READ         1
#define SYS_EXIT         2
#define SYS_GET_TICK     3
#define SYS_FILE_OPEN    4
#define SYS_FILE_READ    5
#define SYS_FILE_WRITE   6
#define SYS_FILE_CLOSE   7
#define SYS_FS_MKDIR     8
#define SYS_FS_CHDIR     9
#define SYS_FS_GETCWD   10
#define SYS_FS_RM       11

/* Modos de abertura (sys_open): */
#define SYS_OPEN_READ    0   /* abre existente p/ leitura */
#define SYS_OPEN_CREATE  1   /* touch: cria se não existir, abre p/ leitura */
#define SYS_OPEN_WRITE   2   /* cria/trunca e abre p/ escrita (echo >) */

void syscall_init(void);

/* ---- terminal ---- */
static inline long sys_write(const char *str, int len) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_WRITE), "b"(str), "c"(len)
                     : "memory");
    return ret;
}

static inline int sys_read(void) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_READ)
                     : "memory");
    return ret;
}

static inline void sys_exit(void) {
    __asm__ volatile("int $0x80" : : "a"(SYS_EXIT) : "memory");
}

static inline uint32_t sys_get_tick(void) {
    uint32_t ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_GET_TICK)
                     : "memory");
    return ret;
}

/* ---- sistema de arquivos ---- */
static inline int sys_open(const char *name, int mode) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_FILE_OPEN), "b"(name), "c"(mode)
                     : "memory");
    return ret;
}

static inline long sys_fread(int fd, char *buf, int len) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_FILE_READ), "b"(fd), "c"(buf), "d"(len)
                     : "memory");
    return ret;
}

static inline long sys_fwrite(int fd, const char *buf, int len) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_FILE_WRITE), "b"(fd), "c"(buf), "d"(len)
                     : "memory");
    return ret;
}

static inline void sys_close(int fd) {
    __asm__ volatile("int $0x80" : : "a"(SYS_FILE_CLOSE), "b"(fd) : "memory");
}

static inline int sys_mkdir(const char *name) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_FS_MKDIR), "b"(name)
                     : "memory");
    return ret;
}

static inline int sys_chdir(const char *name) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_FS_CHDIR), "b"(name)
                     : "memory");
    return ret;
}

static inline int sys_pwd(char *buf, int len) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_FS_GETCWD), "b"(buf), "c"(len)
                     : "memory");
    return ret;
}

static inline int sys_rm(const char *name) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_FS_RM), "b"(name)
                     : "memory");
    return ret;
}

#endif