/* chunk.h -- 4 KB chunk allocator for PL/SW compiler on COR24 */
/* Freestanding -- no libc, no malloc */

/* The chunk allocator owns the only static block in the project
 * larger than 4 KB. Every other static array must stay <= 4 KB
 * or migrate to a chunk. See docs/shrink-lgo-size.md Phase 2.
 *
 * No callers yet -- this header is infrastructure for subsequent
 * sagas (ast-to-chunks, buffer-to-chunks). */

#ifndef CHUNK_H
#define CHUNK_H

/* One chunk == the 4 KB static-block ceiling. */
#define CHUNK_SIZE 4096

/* 128 chunks * 4 KB = 512 KB total dynamic-memory budget for the
 * production compiler. Sized per the 2026-05-12 re-measurement
 * (docs/memory-audit-2026-05-12.md): worst measurable input
 * sno_exec.plsw peaks at 45 chunks; CHUNK_MAX=128 gives 2.85x
 * headroom, exceeds the 332 KB pre-Phase-3 static AST floor by
 * 1.54x, and covers the estimated ~95-chunk sno_engine.plsw
 * peak with 1.35x headroom. Prior CHUNK_MAX=16 (64 KB) was
 * undersized and caused the capacity regression that the
 * capacity-and-test saga fixes. */
#define CHUNK_MAX  128

struct chunk_desc {
    int   in_use;   /* 0 = free, 1 = allocated */
    char *base;     /* start of this chunk's CHUNK_SIZE region */
};

struct chunk_desc chunk_table[CHUNK_MAX];
char chunk_storage[CHUNK_MAX * CHUNK_SIZE]; /* lint-exempt: chunk-pool */

void chunk_init(void) {
    int i;
    i = 0;
    while (i < CHUNK_MAX) {
        chunk_table[i].in_use = 0;
        chunk_table[i].base = chunk_storage + i * CHUNK_SIZE;
        i = i + 1;
    }
}

/* Return base of a free chunk, or 0 (NULL) when exhausted. */
char *chunk_alloc(void) {
    int i;
    i = 0;
    while (i < CHUNK_MAX) {
        if (chunk_table[i].in_use == 0) {
            chunk_table[i].in_use = 1;
            return chunk_table[i].base;
        }
        i = i + 1;
    }
    return 0;
}

/* Mark the chunk owning `base` free. Silent no-op on NULL or any
 * pointer that doesn't match a known chunk base -- the
 * freestanding contract has no trap path, and double-free of an
 * unknown pointer must not corrupt the table. */
void chunk_free(char *base) {
    int i;
    if (base == 0) return;
    i = 0;
    while (i < CHUNK_MAX) {
        if (chunk_table[i].base == base) {
            chunk_table[i].in_use = 0;
            return;
        }
        i = i + 1;
    }
}

int chunk_used(void) {
    int i;
    int n;
    i = 0;
    n = 0;
    while (i < CHUNK_MAX) {
        if (chunk_table[i].in_use != 0) n = n + 1;
        i = i + 1;
    }
    return n;
}

#endif
