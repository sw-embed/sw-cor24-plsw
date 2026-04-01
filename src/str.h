/* str.h -- String and memory utilities for PL/SW compiler on COR24 */
/* Freestanding -- no libc */

#ifndef STR_H
#define STR_H

int str_len(char *s) {
    int n = 0;
    while (*s) {
        n = n + 1;
        s = s + 1;
    }
    return n;
}

void str_copy(char *dst, char *src) {
    while (*src) {
        *dst = *src;
        dst = dst + 1;
        src = src + 1;
    }
    *dst = 0;
}

/* Copy up to max-1 chars, always null-terminate */
void str_ncopy(char *dst, char *src, int max) {
    int i = 0;
    while (*src && i < max - 1) {
        *dst = *src;
        dst = dst + 1;
        src = src + 1;
        i = i + 1;
    }
    *dst = 0;
}

/* Returns 0 if equal, <0 if a<b, >0 if a>b */
int str_cmp(char *a, char *b) {
    while (*a && *b && *a == *b) {
        a = a + 1;
        b = b + 1;
    }
    return *a - *b;
}

/* Returns 1 if equal, 0 otherwise */
int str_eq(char *a, char *b) {
    while (*a && *b && *a == *b) {
        a = a + 1;
        b = b + 1;
    }
    return *a == *b;
}

/* Find substring needle in haystack. Returns pointer to first match, or 0. */
char *str_find(char *haystack, char *needle) {
    char *h;
    char *n;
    char *start;

    if (!*needle) return haystack;

    start = haystack;
    while (*start) {
        h = start;
        n = needle;
        while (*h && *n && *h == *n) {
            h = h + 1;
            n = n + 1;
        }
        if (!*n) return start;
        start = start + 1;
    }
    return 0;
}

void mem_set(char *dst, int val, int n) {
    while (n > 0) {
        *dst = val;
        dst = dst + 1;
        n = n - 1;
    }
}

void mem_copy(char *dst, char *src, int n) {
    while (n > 0) {
        *dst = *src;
        dst = dst + 1;
        src = src + 1;
        n = n - 1;
    }
}

#endif
