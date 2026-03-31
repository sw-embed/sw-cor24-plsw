/* types.h -- Type representation and checking for PL/SW compiler */
/* Builds on TYPE_ constants defined in ast.h */

#ifndef TYPES_H
#define TYPES_H

#include "ast.h"

/* --- Type width --- */

/* Return the width in bytes for a simple type tag */
int type_width(int t) {
    if (t == TYPE_BIT)   return 1;
    if (t == TYPE_INT8)  return 1;
    if (t == TYPE_BYTE)  return 1;
    if (t == TYPE_CHAR)  return 1;
    if (t == TYPE_INT16) return 2;
    if (t == TYPE_WORD)  return 3;
    if (t == TYPE_INT24) return 3;
    if (t == TYPE_PTR)   return 3;
    return 0;  /* TYPE_NONE, TYPE_ARRAY, TYPE_RECORD -- no simple width */
}

/* --- Type classification --- */

int type_is_integer(int t) {
    if (t == TYPE_INT8)  return 1;
    if (t == TYPE_INT16) return 1;
    if (t == TYPE_INT24) return 1;
    if (t == TYPE_BYTE)  return 1;
    if (t == TYPE_CHAR)  return 1;
    if (t == TYPE_BIT)   return 1;
    if (t == TYPE_WORD)  return 1;
    return 0;
}

int type_is_signed(int t) {
    if (t == TYPE_INT8)  return 1;
    if (t == TYPE_INT16) return 1;
    if (t == TYPE_INT24) return 1;
    return 0;
}

int type_is_ptr(int t) {
    return t == TYPE_PTR;
}

int type_is_scalar(int t) {
    return type_is_integer(t) || type_is_ptr(t);
}

/* --- Type descriptor table for compound types --- */
/* Stores extra metadata for ARRAY and RECORD types */

#define TDESC_MAX 64

/* Descriptor kind */
#define TDESC_NONE   0
#define TDESC_ARRAY  1
#define TDESC_RECORD 2

int td_kind[TDESC_MAX];       /* TDESC_ARRAY or TDESC_RECORD */
int td_elem_type[TDESC_MAX];  /* ARRAY: element type tag */
int td_count[TDESC_MAX];      /* ARRAY: element count; RECORD: field count */
int td_size[TDESC_MAX];       /* total size in bytes */

/* Record field info -- parallel arrays, max 16 fields per record */
#define TDESC_FIELD_MAX 16
#define TDESC_FIELDS_TOTAL 1024  /* 64 descriptors * 16 fields */

char *td_fname[TDESC_FIELDS_TOTAL];   /* field name */
int   td_ftype[TDESC_FIELDS_TOTAL];   /* field type tag */
int   td_fwidth[TDESC_FIELDS_TOTAL];  /* field width in bytes */
int   td_foffset[TDESC_FIELDS_TOTAL]; /* field offset from record start */

int td_count_alloc;  /* number of allocated descriptors */

void types_init(void) {
    td_count_alloc = 0;
}

/* Allocate a new type descriptor. Returns index or -1. */
int td_alloc(int kind) {
    if (td_count_alloc >= TDESC_MAX) return -1;
    int i = td_count_alloc;
    td_count_alloc = td_count_alloc + 1;
    td_kind[i] = kind;
    td_elem_type[i] = TYPE_NONE;
    td_count[i] = 0;
    td_size[i] = 0;
    return i;
}

/* Create an array type descriptor.
   Returns descriptor index or -1. */
int td_make_array(int elem_type, int elem_count) {
    int i = td_alloc(TDESC_ARRAY);
    if (i < 0) return -1;
    td_elem_type[i] = elem_type;
    td_count[i] = elem_count;
    td_size[i] = type_width(elem_type) * elem_count;
    return i;
}

/* Create a record type descriptor from DCL AST node.
   Walks the children (field DCL nodes) and computes offsets.
   Returns descriptor index or -1. */
int td_make_record(int dcl_node) {
    int i;
    int fbase;
    int fcount;
    int offset;
    int child;
    int fw;

    i = td_alloc(TDESC_RECORD);
    if (i < 0) return -1;

    fbase = i * TDESC_FIELD_MAX;
    fcount = 0;
    offset = 0;

    /* Walk child fields of the record DCL node */
    child = nd_left[dcl_node];
    while (child != NODE_NULL && fcount < TDESC_FIELD_MAX) {
        fw = type_width(nd_type[child]);
        /* Arrays: width = element_width * dimension */
        if (nd_ival[child] > 0) {
            fw = fw * nd_ival[child];
        }
        if (fw == 0) fw = 3;  /* default to 24-bit for untyped fields */

        td_fname[fbase + fcount] = nd_name[child];
        td_ftype[fbase + fcount] = nd_type[child];
        td_fwidth[fbase + fcount] = fw;
        td_foffset[fbase + fcount] = offset;

        offset = offset + fw;
        fcount = fcount + 1;
        child = nd_next[child];
    }

    td_count[i] = fcount;
    td_size[i] = offset;
    return i;
}

/* Look up a field in a record descriptor by name.
   Returns the field index (within descriptor) or -1 if not found. */
int td_field_lookup(int desc, char *name) {
    int fbase;
    int fc;
    int j;

    if (desc < 0 || desc >= td_count_alloc) return -1;
    if (td_kind[desc] != TDESC_RECORD) return -1;

    fbase = desc * TDESC_FIELD_MAX;
    fc = td_count[desc];
    j = 0;
    while (j < fc) {
        if (str_eq_nocase(td_fname[fbase + j], name)) {
            return j;
        }
        j = j + 1;
    }
    return -1;
}

/* Get field offset within a record descriptor */
int td_field_offset(int desc, int field_idx) {
    int fbase;
    if (desc < 0 || field_idx < 0) return 0;
    fbase = desc * TDESC_FIELD_MAX;
    return td_foffset[fbase + field_idx];
}

/* Get field type within a record descriptor */
int td_field_type(int desc, int field_idx) {
    int fbase;
    if (desc < 0 || field_idx < 0) return TYPE_NONE;
    fbase = desc * TDESC_FIELD_MAX;
    return td_ftype[fbase + field_idx];
}

/* Get field width within a record descriptor */
int td_field_width(int desc, int field_idx) {
    int fbase;
    if (desc < 0 || field_idx < 0) return 0;
    fbase = desc * TDESC_FIELD_MAX;
    return td_fwidth[fbase + field_idx];
}

/* --- Type compatibility checking --- */

/* Type checking error state */
int tc_err;
char *tc_errmsg;

void tc_init(void) {
    tc_err = 0;
    tc_errmsg = 0;
}

void tc_error(char *msg) {
    tc_err = 1;
    tc_errmsg = msg;
}

/* Check if value of type 'from' can be assigned to variable of type 'to'.
   Returns 1 if compatible, 0 if not (and sets tc_err). */
int type_compat_assign(int from, int to) {
    /* Same type is always compatible */
    if (from == to) return 1;

    /* Any integer type can be assigned to any other integer type
       (implicit widening/narrowing, PL/I style) */
    if (type_is_integer(from) && type_is_integer(to)) return 1;

    /* Integer can be assigned to PTR (address literal) */
    if (type_is_integer(from) && to == TYPE_PTR) return 1;

    /* PTR can be assigned to integer (cast) */
    if (from == TYPE_PTR && type_is_integer(to)) return 1;

    /* PTR to PTR */
    if (from == TYPE_PTR && to == TYPE_PTR) return 1;

    /* NONE type -- untyped, compatible with anything */
    if (from == TYPE_NONE || to == TYPE_NONE) return 1;

    tc_error("incompatible types in assignment");
    return 0;
}

/* Determine the result type of a binary operation.
   Returns the result type tag, or TYPE_NONE on error. */
int type_binop_result(int op, int left, int right) {
    /* Comparison operators always produce INT24 (boolean result) */
    if (op == TOK_EQ || op == TOK_NE ||
        op == TOK_LT || op == TOK_GT ||
        op == TOK_LE || op == TOK_GE) {
        /* Both sides must be scalar */
        if (!type_is_scalar(left) || !type_is_scalar(right)) {
            tc_error("comparison requires scalar operands");
            return TYPE_NONE;
        }
        return TYPE_INT24;
    }

    /* Logical operators produce INT24 */
    if (op == TOK_AND || op == TOK_OR || op == TOK_NOT) {
        return TYPE_INT24;
    }

    /* Pointer arithmetic: PTR + INT or PTR - INT => PTR */
    if (op == TOK_PLUS || op == TOK_MINUS) {
        if (type_is_ptr(left) && type_is_integer(right)) return TYPE_PTR;
        if (type_is_integer(left) && type_is_ptr(right) && op == TOK_PLUS) return TYPE_PTR;
        /* PTR - PTR => INT24 (distance) */
        if (type_is_ptr(left) && type_is_ptr(right) && op == TOK_MINUS) return TYPE_INT24;
    }

    /* Arithmetic on integers: promote to widest */
    if (type_is_integer(left) && type_is_integer(right)) {
        int wl = type_width(left);
        int wr = type_width(right);
        if (wl >= wr) return left;
        return right;
    }

    /* NONE types are permissive */
    if (left == TYPE_NONE || right == TYPE_NONE) return TYPE_INT24;

    tc_error("invalid operand types for operator");
    return TYPE_NONE;
}

/* Determine result type of a unary operation */
int type_unop_result(int op, int operand) {
    if (op == TOK_MINUS) {
        if (type_is_integer(operand)) return operand;
        tc_error("unary minus requires integer operand");
        return TYPE_NONE;
    }
    if (op == TOK_TILDE) {
        if (type_is_integer(operand)) return operand;
        tc_error("bitwise NOT requires integer operand");
        return TYPE_NONE;
    }
    if (op == TOK_NOT) {
        return TYPE_INT24;
    }
    return operand;
}

/* Check procedure argument compatibility.
   param_type = declared parameter type, arg_type = actual argument type.
   Returns 1 if compatible, 0 if not (and sets tc_err). */
int type_compat_arg(int arg_type, int param_type) {
    /* Same rules as assignment */
    return type_compat_assign(arg_type, param_type);
}

/* --- Type descriptor debug output --- */

void td_print(int desc) {
    int fbase;
    int fc;
    int j;

    if (desc < 0 || desc >= td_count_alloc) {
        uart_puts("  (invalid descriptor)");
        return;
    }

    if (td_kind[desc] == TDESC_ARRAY) {
        uart_putstr("  ARRAY elem=");
        uart_putstr(nd_type_name(td_elem_type[desc]));
        uart_putstr(" count=");
        print_int(td_count[desc]);
        uart_putstr(" size=");
        print_int(td_size[desc]);
        uart_putchar(10);
    } else if (td_kind[desc] == TDESC_RECORD) {
        uart_putstr("  RECORD fields=");
        print_int(td_count[desc]);
        uart_putstr(" size=");
        print_int(td_size[desc]);
        uart_putchar(10);

        fbase = desc * TDESC_FIELD_MAX;
        fc = td_count[desc];
        j = 0;
        while (j < fc) {
            uart_putstr("    ");
            if (td_fname[fbase + j]) {
                uart_putstr(td_fname[fbase + j]);
            } else {
                uart_putstr("(null)");
            }
            uart_putstr(" type=");
            uart_putstr(nd_type_name(td_ftype[fbase + j]));
            uart_putstr(" w=");
            print_int(td_fwidth[fbase + j]);
            uart_putstr(" off=");
            print_int(td_foffset[fbase + j]);
            uart_putchar(10);
            j = j + 1;
        }
    }
}

#endif
