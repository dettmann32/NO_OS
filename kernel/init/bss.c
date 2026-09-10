#include "../include/bss.h"

extern char __bss_start;
extern char __bss_end;

void init_bss(void) {
    char *bss_start = &__bss_start;
    char *bss_end = &__bss_end;

    while (bss_start < bss_end) {
        *bss_start++ = 0;
    }
}