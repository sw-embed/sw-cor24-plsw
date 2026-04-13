/* symtab.h -- Scoped symbol table for PL/SW compiler */
/* Parallel arrays (tc24r workaround), linear scan lookup */

#ifndef SYMTAB_H
#define SYMTAB_H

#include "arena.h"

/* Max symbols per scope */
#define SYM_SCOPE_MAX 256

/* Max scope nesting depth (global + procedures + blocks) */
#define SYM_DEPTH_MAX 8

/* Symbol fields -- parallel arrays per scope */
/* We flatten all scopes into one big table: 8 * 256 = 2048 entries */
#define SYM_TOTAL 2048

char *sym_name[SYM_TOTAL];    /* symbol name (arena-allocated) */
char *sym_asm_name[SYM_TOTAL]; /* assembly label (mangled), or 0 = use sym_name */
int   sym_type[SYM_TOTAL];    /* type tag (TYPE_INT8, TYPE_PTR, etc.) */
int   sym_width[SYM_TOTAL];   /* width in bytes */
int   sym_stor[SYM_TOTAL];    /* storage class (STOR_AUTO, STOR_STATIC, ...) */
int   sym_offset[SYM_TOTAL];  /* stack offset or static address */
int   sym_level[SYM_TOTAL];   /* DCL level (1,3,5,...) for records */
int   sym_flags[SYM_TOTAL];   /* misc flags (e.g. is_param, is_proc) */
int   sym_tdesc[SYM_TOTAL];  /* type descriptor index (for records/arrays), or -1 */

/* Flag bits */
#define SYM_F_PARAM  1   /* symbol is a procedure parameter */
#define SYM_F_PROC   2   /* symbol is a procedure name */
#define SYM_F_ARRAY  4   /* symbol is an array */
#define SYM_F_RECORD 8   /* symbol is a record */

/* Per-scope tracking */
int sym_scope_base[SYM_DEPTH_MAX];  /* base index into sym_* arrays */
int sym_scope_count[SYM_DEPTH_MAX]; /* number of symbols in this scope */

int sym_depth;    /* current scope depth (0 = global) */

/* Error state */
int sym_err;
char *sym_errmsg;

void sym_init(void) {
    sym_depth = 0;
    sym_scope_base[0] = 0;
    sym_scope_count[0] = 0;
    sym_err = 0;
    sym_errmsg = 0;
}

/* Enter a new scope */
void sym_enter_scope(void) {
    if (sym_depth + 1 >= SYM_DEPTH_MAX) {
        sym_err = 1;
        sym_errmsg = "scope nesting too deep";
        return;
    }
    int new_base;
    /* New scope starts after current scope's symbols */
    new_base = sym_scope_base[sym_depth] + sym_scope_count[sym_depth];
    sym_depth = sym_depth + 1;
    sym_scope_base[sym_depth] = new_base;
    sym_scope_count[sym_depth] = 0;
}

/* Exit current scope (discards its symbols) */
void sym_exit_scope(void) {
    if (sym_depth <= 0) {
        sym_err = 1;
        sym_errmsg = "cannot exit global scope";
        return;
    }
    sym_depth = sym_depth - 1;
}

/* Insert a symbol into the current scope.
   Returns the index, or -1 on error. */
int sym_insert(char *name, int type, int width, int stor) {
    int base;
    int count;
    int i;
    int idx;
    char *s;
    int len;

    base = sym_scope_base[sym_depth];
    count = sym_scope_count[sym_depth];

    /* Check for duplicate in current scope */
    i = 0;
    while (i < count) {
        idx = base + i;
        if (str_eq_nocase(sym_name[idx], name)) {
            sym_err = 1;
            sym_errmsg = "duplicate symbol";
            return -1;
        }
        i = i + 1;
    }

    /* Check capacity */
    if (count >= SYM_SCOPE_MAX) {
        sym_err = 1;
        sym_errmsg = "too many symbols in scope";
        return -1;
    }

    if (base + count >= SYM_TOTAL) {
        sym_err = 1;
        sym_errmsg = "symbol table full";
        return -1;
    }

    /* Allocate the entry */
    idx = base + count;
    sym_scope_count[sym_depth] = count + 1;

    /* Copy name into arena */
    len = str_len(name);
    s = arena_alloc(len + 1);
    if (s) {
        str_copy(s, name);
    }
    sym_name[idx] = s;
    sym_asm_name[idx] = 0;
    sym_type[idx] = type;
    sym_width[idx] = width;
    sym_stor[idx] = stor;
    sym_offset[idx] = 0;
    sym_level[idx] = 0;
    sym_flags[idx] = 0;
    sym_tdesc[idx] = -1;

    return idx;
}

/* Look up a symbol by name, searching from current scope outward.
   Returns the index, or -1 if not found. */
int sym_lookup(char *name) {
    int d;
    int base;
    int count;
    int i;
    int idx;

    d = sym_depth;
    while (d >= 0) {
        base = sym_scope_base[d];
        count = sym_scope_count[d];
        i = 0;
        while (i < count) {
            idx = base + i;
            if (str_eq_nocase(sym_name[idx], name)) {
                return idx;
            }
            i = i + 1;
        }
        d = d - 1;
    }
    return -1;
}

/* Look up a symbol only in the current scope.
   Returns the index, or -1 if not found. */
int sym_lookup_local(char *name) {
    int base;
    int count;
    int i;
    int idx;

    base = sym_scope_base[sym_depth];
    count = sym_scope_count[sym_depth];
    i = 0;
    while (i < count) {
        idx = base + i;
        if (str_eq_nocase(sym_name[idx], name)) {
            return idx;
        }
        i = i + 1;
    }
    return -1;
}

/* Return the current scope depth */
int sym_get_depth(void) {
    return sym_depth;
}

/* Return the number of symbols in the current scope */
int sym_scope_size(void) {
    return sym_scope_count[sym_depth];
}

/* Print a symbol for debugging */
void sym_print(int idx) {
    if (idx < 0 || idx >= SYM_TOTAL) return;
    uart_putstr("  ");
    if (sym_name[idx]) {
        uart_putstr(sym_name[idx]);
    } else {
        uart_putstr("(null)");
    }
    uart_putstr(" type=");
    uart_putstr(nd_type_name(sym_type[idx]));
    uart_putstr(" w=");
    print_int(sym_width[idx]);
    if (sym_stor[idx] == STOR_STATIC) uart_putstr(" STATIC");
    if (sym_stor[idx] == STOR_EXTERNAL) uart_putstr(" EXTERNAL");
    if (sym_stor[idx] == STOR_BASED) uart_putstr(" BASED");
    if (sym_offset[idx] != 0) {
        uart_putstr(" off=");
        print_int(sym_offset[idx]);
    }
    if (sym_flags[idx] & SYM_F_PARAM) uart_putstr(" PARAM");
    if (sym_flags[idx] & SYM_F_PROC) uart_putstr(" PROC");
    if (sym_flags[idx] & SYM_F_ARRAY) uart_putstr(" ARRAY");
    if (sym_flags[idx] & SYM_F_RECORD) uart_putstr(" RECORD");
    uart_putchar(10);
}

/* Dump all symbols in the current scope */
void sym_dump_scope(void) {
    int base;
    int count;
    int i;

    base = sym_scope_base[sym_depth];
    count = sym_scope_count[sym_depth];
    uart_putstr("scope depth=");
    print_int(sym_depth);
    uart_putstr(" count=");
    print_int(count);
    uart_putchar(10);
    i = 0;
    while (i < count) {
        sym_print(base + i);
        i = i + 1;
    }
}

/* Dump all scopes (for debugging) */
void sym_dump_all(void) {
    int d;
    int base;
    int count;
    int i;

    d = 0;
    while (d <= sym_depth) {
        base = sym_scope_base[d];
        count = sym_scope_count[d];
        uart_putstr("scope[");
        print_int(d);
        uart_putstr("] base=");
        print_int(base);
        uart_putstr(" count=");
        print_int(count);
        uart_putchar(10);
        i = 0;
        while (i < count) {
            sym_print(base + i);
            i = i + 1;
        }
        d = d + 1;
    }
}

#endif
