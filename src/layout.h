/* layout.h -- Storage layout computation for PL/SW compiler */
/* Walks DCL AST nodes to assign offsets, compute sizes, build type descriptors */

#ifndef LAYOUT_H
#define LAYOUT_H

#include "ast.h"
#include "symtab.h"
#include "types.h"

/* --- Static address allocation --- */

/* Base address for static variables in .data section */
#define STATIC_BASE 0x1000

int layout_static_next;  /* next available static address */

/* --- Frame layout state --- */

int layout_frame_size;   /* total frame size for current procedure */

/* --- Last allocated type descriptor (set by layout_dcl_width for records) --- */
int layout_last_tdesc;

/* --- Layout error state --- */

int layout_err;
char *layout_errmsg;

void layout_init(void) {
    layout_static_next = STATIC_BASE;
    layout_frame_size = 0;
    layout_last_tdesc = -1;
    layout_err = 0;
    layout_errmsg = 0;
}

void layout_error(char *msg) {
    layout_err = 1;
    layout_errmsg = msg;
}

/* --- Compute the width of a DCL node --- */
/* Handles simple types, arrays, and records.
   For records, also builds a type descriptor.
   Returns the total width in bytes. */

int layout_dcl_width(int node) {
    int w;
    int child;
    int fcount;
    int offset;
    int fw;
    int desc;
    int fbase;

    if (node == NODE_NULL) return 0;

    /* Record type: walk children to compute total size */
    if (nd_type[node] == TYPE_RECORD) {
        layout_last_tdesc = -1;
        offset = 0;
        fcount = 0;

        /* Allocate a type descriptor for this record */
        desc = td_alloc(TDESC_RECORD);
        if (desc >= 0) {
            fbase = desc * TDESC_FIELD_MAX;

            child = nd_left[node];
            while (child != NODE_NULL && fcount < TDESC_FIELD_MAX) {
                fw = layout_dcl_width(child);
                if (fw == 0) fw = 3;  /* default to 24-bit */

                td_fname[fbase + fcount] = nd_name[child];
                td_ftype[fbase + fcount] = nd_type[child];
                td_fwidth[fbase + fcount] = fw;
                td_foffset[fbase + fcount] = offset;

                offset = offset + fw;
                fcount = fcount + 1;
                child = nd_next[child];
            }

            td_count[desc] = fcount;
            td_size[desc] = offset;
            layout_last_tdesc = desc;
        }
        return offset;
    }

    /* Simple type width */
    w = type_width(nd_type[node]);
    if (w == 0) w = 3;  /* default to 24-bit for untyped */

    /* Array: width = element_width * dimension */
    if (nd_ival[node] > 0) {
        w = w * nd_ival[node];
    }

    return w;
}

/* --- Lay out parameters for a procedure --- */
/* Parameters are at positive offsets from fp.
   COR24 calling convention: first arg at fp+9 (3 saved regs * 3 bytes).
   Subsequent args follow at increasing offsets. */

#define PARAM_BASE_OFFSET 9  /* fp + 9 for first parameter */

void layout_params(int first_param) {
    int p;
    int offset;
    int idx;
    int w;

    p = first_param;
    offset = PARAM_BASE_OFFSET;

    while (p != NODE_NULL) {
        w = type_width(nd_type[p]);
        if (w == 0) w = 3;

        /* Insert parameter into symbol table */
        idx = sym_insert(nd_name[p], nd_type[p], w, STOR_AUTO);
        if (idx >= 0) {
            sym_offset[idx] = offset;
            sym_flags[idx] = sym_flags[idx] | SYM_F_PARAM;
        }

        /* Parameters always occupy 3-byte stack slots (push is 3 bytes) */
        offset = offset + 3;
        p = nd_next[p];
    }
}

/* --- Lay out local declarations in a procedure body --- */
/* AUTOMATIC locals get negative offsets from fp.
   STATIC locals get addresses from the static area. */

void layout_locals(int body_node) {
    int stmt;
    int w;
    int idx;
    int desc;

    if (body_node == NODE_NULL) return;

    /* If this node is itself a statement (not a BLOCK), handle it directly */
    if (nd_kind[body_node] == NODE_IF) {
        if (nd_right[body_node] != NODE_NULL)
            layout_locals(nd_right[body_node]);
        if (nd_ival[body_node] != NODE_NULL)
            layout_locals(nd_ival[body_node]);
        return;
    }
    if (nd_kind[body_node] == NODE_DO_WHILE || nd_kind[body_node] == NODE_DO_COUNT) {
        if (nd_right[body_node] != NODE_NULL)
            layout_locals(nd_right[body_node]);
        return;
    }

    /* Walk statements in the body block */
    stmt = nd_left[body_node];
    while (stmt != NODE_NULL) {
        if (nd_kind[stmt] == NODE_DCL) {
            w = layout_dcl_width(stmt);

            /* Insert into symbol table */
            idx = sym_insert(nd_name[stmt], nd_type[stmt], w, nd_stor[stmt]);
            if (idx >= 0) {
                if (nd_stor[stmt] == STOR_STATIC) {
                    /* Static: assign address from static area */
                    sym_offset[idx] = layout_static_next;
                    layout_static_next = layout_static_next + w;
                } else {
                    /* Automatic: negative offset from fp */
                    layout_frame_size = layout_frame_size + w;
                    sym_offset[idx] = 0 - layout_frame_size;
                }

                /* Set flags and type descriptor for compound types */
                if (nd_type[stmt] == TYPE_RECORD) {
                    sym_flags[idx] = sym_flags[idx] | SYM_F_RECORD;
                    sym_tdesc[idx] = layout_last_tdesc;
                }
                if (nd_ival[stmt] > 0) {
                    sym_flags[idx] = sym_flags[idx] | SYM_F_ARRAY;
                }
                /* PTR: associate with most recent record type descriptor */
                if (nd_type[stmt] == TYPE_PTR && layout_last_tdesc >= 0) {
                    sym_tdesc[idx] = layout_last_tdesc;
                }
            }
        }
        /* Recurse into nested blocks to find DCLs */
        if (nd_kind[stmt] == NODE_BLOCK) {
            layout_locals(stmt);
        } else if (nd_kind[stmt] == NODE_IF) {
            /* then-body is nd_right, else-body is nd_ival */
            if (nd_right[stmt] != NODE_NULL)
                layout_locals(nd_right[stmt]);
            if (nd_ival[stmt] != NODE_NULL)
                layout_locals(nd_ival[stmt]);
        } else if (nd_kind[stmt] == NODE_DO_WHILE) {
            if (nd_right[stmt] != NODE_NULL)
                layout_locals(nd_right[stmt]);
        } else if (nd_kind[stmt] == NODE_DO_COUNT) {
            if (nd_right[stmt] != NODE_NULL)
                layout_locals(nd_right[stmt]);
        }
        stmt = nd_next[stmt];
    }
}

/* --- Lay out a complete procedure --- */
/* Enters a new scope, processes params and locals,
   returns the frame size. Does NOT exit the scope
   (caller should do that after codegen). */

int layout_proc(int proc_node) {
    if (proc_node == NODE_NULL) return 0;

    layout_frame_size = 0;
    sym_enter_scope();

    /* Lay out parameters */
    layout_params(nd_left[proc_node]);

    /* Lay out local declarations in body */
    layout_locals(nd_right[proc_node]);

    return layout_frame_size;
}

/* --- Lay out global declarations --- */
/* Top-level DCLs: STATIC get static addresses,
   others treated as static by default at global scope. */

void layout_globals(int prog_node) {
    int child;
    int w;
    int idx;

    if (prog_node == NODE_NULL) return;

    child = nd_left[prog_node];
    while (child != NODE_NULL) {
        if (nd_kind[child] == NODE_DCL) {
            w = layout_dcl_width(child);

            idx = sym_insert(nd_name[child], nd_type[child], w,
                             nd_stor[child] == STOR_AUTO ? STOR_STATIC : nd_stor[child]);
            if (idx >= 0) {
                sym_offset[idx] = layout_static_next;
                layout_static_next = layout_static_next + w;

                if (nd_type[child] == TYPE_RECORD) {
                    sym_flags[idx] = sym_flags[idx] | SYM_F_RECORD;
                    sym_tdesc[idx] = layout_last_tdesc;
                }
                if (nd_ival[child] > 0) {
                    sym_flags[idx] = sym_flags[idx] | SYM_F_ARRAY;
                }
                /* PTR: associate with most recent record type descriptor */
                if (nd_type[child] == TYPE_PTR && layout_last_tdesc >= 0) {
                    sym_tdesc[idx] = layout_last_tdesc;
                }
            }
        }
        child = nd_next[child];
    }
}

/* --- Debug output --- */

void layout_print_frame(void) {
    uart_putstr("  frame_size=");
    print_int(layout_frame_size);
    uart_putstr(" static_next=0x");
    /* Print hex manually */
    int v = layout_static_next;
    int d3 = v / 4096;
    int d2 = (v / 256) % 16;
    int d1 = (v / 16) % 16;
    int d0 = v % 16;
    char hex[5];
    hex[0] = d3 < 10 ? '0' + d3 : 'A' + d3 - 10;
    hex[1] = d2 < 10 ? '0' + d2 : 'A' + d2 - 10;
    hex[2] = d1 < 10 ? '0' + d1 : 'A' + d1 - 10;
    hex[3] = d0 < 10 ? '0' + d0 : 'A' + d0 - 10;
    hex[4] = 0;
    uart_putstr(hex);
    uart_putchar(10);
}

#endif
