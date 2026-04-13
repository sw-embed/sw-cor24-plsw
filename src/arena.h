/* arena.h -- Bump allocator for PL/SW compiler tables on COR24 */
/* Freestanding -- no libc, no malloc */

#ifndef ARENA_H
#define ARENA_H

#include "str.h"

/* Arena size in bytes -- 24KB for compiler tables */
#define ARENA_SIZE 24576

char arena_buf[ARENA_SIZE];
int arena_top;

void arena_init(void) {
    arena_top = 0;
}

/* Allocate n bytes from the arena. Returns pointer or 0 on OOM. */
char *arena_alloc(int n) {
    if (arena_top + n > ARENA_SIZE) {
        return 0;
    }
    char *p = arena_buf + arena_top;
    arena_top = arena_top + n;
    /* Zero-fill the allocation */
    mem_set(p, 0, n);
    return p;
}

/* Return current watermark (for save/restore) */
int arena_save(void) {
    return arena_top;
}

/* Restore arena to a saved watermark */
void arena_restore(int mark) {
    arena_top = mark;
}

/* How many bytes remain */
int arena_free(void) {
    return ARENA_SIZE - arena_top;
}

#endif
