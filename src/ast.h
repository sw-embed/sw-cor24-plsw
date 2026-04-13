/* ast.h -- AST node types and pool for PL/SW compiler */
/* Uses parallel arrays instead of structs (tc24r workaround) */

#ifndef AST_H
#define AST_H

#include "arena.h"

/* Node kind constants */
#define NODE_NONE          0
#define NODE_PROGRAM       1
#define NODE_DCL           2
#define NODE_PROC          3
#define NODE_IF            4
#define NODE_DO_WHILE      5
#define NODE_DO_COUNT      6
#define NODE_ASSIGN        7
#define NODE_BINOP         8
#define NODE_UNOP          9
#define NODE_CALL         10
#define NODE_RETURN       11
#define NODE_LITERAL      12
#define NODE_IDENT        13
#define NODE_ASM_BLOCK    14
#define NODE_FIELD_ACCESS 15
#define NODE_ARRAY_ACCESS 16
#define NODE_ADDR         17
#define NODE_DEREF        18
#define NODE_BLOCK        19
#define NODE_PARAM        20
#define NODE_SELECT       21

/* Type info constants */
#define TYPE_NONE    0
#define TYPE_INT8    1
#define TYPE_INT16   2
#define TYPE_INT24   3
#define TYPE_BYTE    4
#define TYPE_CHAR    5
#define TYPE_BIT     6
#define TYPE_WORD    7
#define TYPE_PTR     8
#define TYPE_ARRAY   9
#define TYPE_RECORD  10

/* Storage class constants */
#define STOR_AUTO     0
#define STOR_STATIC   1
#define STOR_EXTERNAL 2
#define STOR_BASED    3

/* Null node index sentinel */
#define NODE_NULL (-1)

/* Node pool -- parallel arrays */
#define NODE_POOL_MAX 12288

int nd_kind[NODE_POOL_MAX];       /* node kind */
int nd_type[NODE_POOL_MAX];       /* type info */
int nd_stor[NODE_POOL_MAX];       /* storage class */
int nd_level[NODE_POOL_MAX];      /* DCL level (1,3,5,...) */
int nd_left[NODE_POOL_MAX];       /* left child index */
int nd_right[NODE_POOL_MAX];      /* right child index */
int nd_next[NODE_POOL_MAX];       /* sibling/next index */
int nd_ival[NODE_POOL_MAX];       /* integer value or operator */
int nd_line[NODE_POOL_MAX];       /* source line number (1-based) */
char *nd_name[NODE_POOL_MAX];     /* name (arena-allocated) */

int nd_count;                     /* number of allocated nodes */

void ast_init(void) {
    nd_count = 0;
    ast_pool_exhausted = 0;
}

/* Allocate a new node. Returns node index or NODE_NULL on OOM. */
int ast_pool_exhausted;

int nd_alloc(int kind) {
    if (nd_count >= NODE_POOL_MAX) {
        if (!ast_pool_exhausted) {
            ast_pool_exhausted = 1;
            uart_puts("ERROR: AST node pool exhausted (12288 nodes)");
            uart_puts("Program too large for single compilation unit.");
        }
        return NODE_NULL;
    }
    int i = nd_count;
    nd_count = nd_count + 1;
    nd_kind[i]  = kind;
    nd_type[i]  = TYPE_NONE;
    nd_stor[i]  = STOR_AUTO;
    nd_level[i] = 0;
    nd_left[i]  = NODE_NULL;
    nd_right[i] = NODE_NULL;
    nd_next[i]  = NODE_NULL;
    nd_ival[i]  = 0;
    nd_line[i]  = lex_line;
    nd_name[i]  = 0;
    return i;
}

/* Set the name on a node (copies string into arena) */
void nd_set_name(int n, char *name) {
    if (n < 0 || n >= nd_count) return;
    int len = str_len(name);
    char *s = arena_alloc(len + 1);
    if (s) {
        str_copy(s, name);
        nd_name[n] = s;
    }
}

/* Append child to end of a node's sibling chain via nd_next */
void nd_append(int parent, int child) {
    if (parent < 0 || child < 0) return;
    /* Find the appropriate child slot */
    if (nd_left[parent] == NODE_NULL) {
        nd_left[parent] = child;
        return;
    }
    /* Walk the sibling chain from left child */
    int cur = nd_left[parent];
    while (nd_next[cur] != NODE_NULL) {
        cur = nd_next[cur];
    }
    nd_next[cur] = child;
}

/* Create a literal node */
int nd_literal(int value) {
    int n = nd_alloc(NODE_LITERAL);
    if (n != NODE_NULL) {
        nd_ival[n] = value;
        nd_type[n] = TYPE_INT24;
    }
    return n;
}

/* Create an identifier node */
int nd_ident(char *name) {
    int n = nd_alloc(NODE_IDENT);
    if (n != NODE_NULL) {
        nd_set_name(n, name);
    }
    return n;
}

/* Create a binary operator node */
int nd_binop(int op, int left, int right) {
    int n = nd_alloc(NODE_BINOP);
    if (n != NODE_NULL) {
        nd_ival[n] = op;
        nd_left[n] = left;
        nd_right[n] = right;
    }
    return n;
}

/* Create a unary operator node */
int nd_unop(int op, int operand) {
    int n = nd_alloc(NODE_UNOP);
    if (n != NODE_NULL) {
        nd_ival[n] = op;
        nd_left[n] = operand;
    }
    return n;
}

/* Create an assignment node */
int nd_assign(int target, int value) {
    int n = nd_alloc(NODE_ASSIGN);
    if (n != NODE_NULL) {
        nd_left[n] = target;
        nd_right[n] = value;
    }
    return n;
}

/* Create a CALL node */
int nd_call(char *name, int args) {
    int n = nd_alloc(NODE_CALL);
    if (n != NODE_NULL) {
        nd_set_name(n, name);
        nd_left[n] = args;
    }
    return n;
}

/* Create a RETURN node */
int nd_return(int expr) {
    int n = nd_alloc(NODE_RETURN);
    if (n != NODE_NULL) {
        nd_left[n] = expr;
    }
    return n;
}

/* Return a human-readable name for a node kind */
char *nd_kind_name(int kind) {
    if (kind == NODE_PROGRAM)       return "PROGRAM";
    if (kind == NODE_DCL)           return "DCL";
    if (kind == NODE_PROC)          return "PROC";
    if (kind == NODE_IF)            return "IF";
    if (kind == NODE_DO_WHILE)      return "DO_WHILE";
    if (kind == NODE_DO_COUNT)      return "DO_COUNT";
    if (kind == NODE_ASSIGN)        return "ASSIGN";
    if (kind == NODE_BINOP)         return "BINOP";
    if (kind == NODE_UNOP)          return "UNOP";
    if (kind == NODE_CALL)          return "CALL";
    if (kind == NODE_RETURN)        return "RETURN";
    if (kind == NODE_LITERAL)       return "LITERAL";
    if (kind == NODE_IDENT)         return "IDENT";
    if (kind == NODE_ASM_BLOCK)     return "ASM_BLOCK";
    if (kind == NODE_FIELD_ACCESS)  return "FIELD_ACCESS";
    if (kind == NODE_ARRAY_ACCESS)  return "ARRAY_ACCESS";
    if (kind == NODE_ADDR)          return "ADDR";
    if (kind == NODE_DEREF)         return "DEREF";
    if (kind == NODE_BLOCK)         return "BLOCK";
    if (kind == NODE_PARAM)        return "PARAM";
    if (kind == NODE_SELECT)       return "SELECT";
    return "???";
}

/* Return a human-readable name for a type */
char *nd_type_name(int t) {
    if (t == TYPE_NONE)   return "NONE";
    if (t == TYPE_INT8)   return "INT8";
    if (t == TYPE_INT16)  return "INT16";
    if (t == TYPE_INT24)  return "INT24";
    if (t == TYPE_BYTE)   return "BYTE";
    if (t == TYPE_CHAR)   return "CHAR";
    if (t == TYPE_BIT)    return "BIT";
    if (t == TYPE_WORD)   return "WORD";
    if (t == TYPE_PTR)    return "PTR";
    if (t == TYPE_ARRAY)  return "ARRAY";
    if (t == TYPE_RECORD) return "RECORD";
    return "???";
}

/* Print a single node for debugging */
void nd_print(int n, int depth) {
    if (n == NODE_NULL) return;
    int d = 0;
    while (d < depth) {
        uart_putstr("  ");
        d = d + 1;
    }
    uart_putstr(nd_kind_name(nd_kind[n]));
    if (nd_name[n]) {
        uart_putstr(" name=");
        uart_putstr(nd_name[n]);
    }
    if (nd_kind[n] == NODE_LITERAL) {
        uart_putstr(" val=");
        print_int(nd_ival[n]);
    }
    if (nd_kind[n] == NODE_BINOP || nd_kind[n] == NODE_UNOP) {
        uart_putstr(" op=");
        uart_putstr(tok_name(nd_ival[n]));
    }
    if (nd_type[n] != TYPE_NONE) {
        uart_putstr(" type=");
        uart_putstr(nd_type_name(nd_type[n]));
    }
    if (nd_kind[n] == NODE_DCL) {
        if (nd_stor[n] == STOR_STATIC) uart_putstr(" STATIC");
        if (nd_stor[n] == STOR_EXTERNAL) uart_putstr(" EXTERNAL");
        if (nd_level[n] > 0) {
            uart_putstr(" lv=");
            print_int(nd_level[n]);
        }
        if (nd_ival[n] > 0) {
            uart_putstr(" dim=");
            print_int(nd_ival[n]);
        }
    }
    if (nd_kind[n] == NODE_PROC) {
        /* nd_ival stores option flags (bit 0=FREESTANDING, 1=NAKED, 2=LEAF) */
        if (nd_ival[n] & 1) uart_putstr(" FREESTANDING");
        if (nd_ival[n] & 2) uart_putstr(" NAKED");
        if (nd_ival[n] & 4) uart_putstr(" LEAF");
        /* nd_stor stores return type */
        if (nd_stor[n] != TYPE_NONE) {
            uart_putstr(" returns=");
            uart_putstr(nd_type_name(nd_stor[n]));
        }
    }
    uart_putchar(10);
}

/* Recursively dump a tree */
void nd_dump(int n, int depth) {
    int d;
    int p;
    if (n == NODE_NULL) return;
    nd_print(n, depth);
    /* PROC: dump params then body with labels */
    if (nd_kind[n] == NODE_PROC) {
        /* Dump parameter list */
        p = nd_left[n];
        if (p != NODE_NULL) {
            d = 0;
            while (d < depth + 1) { uart_putstr("  "); d = d + 1; }
            uart_puts("(PARAMS)");
            while (p != NODE_NULL) {
                nd_print(p, depth + 2);
                p = nd_next[p];
            }
        }
        /* Dump body */
        if (nd_right[n] != NODE_NULL) {
            d = 0;
            while (d < depth + 1) { uart_putstr("  "); d = d + 1; }
            uart_puts("(BODY)");
            nd_dump(nd_right[n], depth + 2);
        }
        nd_dump(nd_next[n], depth);
        return;
    }
    nd_dump(nd_left[n], depth + 1);
    nd_dump(nd_right[n], depth + 1);
    /* IF: dump else branch stored in nd_ival */
    if (nd_kind[n] == NODE_IF && nd_ival[n] != NODE_NULL) {
        d = 0;
        while (d < depth + 1) { uart_putstr("  "); d = d + 1; }
        uart_puts("(ELSE)");
        nd_dump(nd_ival[n], depth + 1);
    }
    /* DO_COUNT: dump end expression stored in nd_ival */
    if (nd_kind[n] == NODE_DO_COUNT && nd_ival[n] != NODE_NULL) {
        d = 0;
        while (d < depth + 1) { uart_putstr("  "); d = d + 1; }
        uart_puts("(TO)");
        nd_dump(nd_ival[n], depth + 1);
    }
    /* SELECT: dump WHEN children via nd_left sibling chain,
       OTHERWISE body stored in nd_ival */
    if (nd_kind[n] == NODE_SELECT) {
        if (nd_ival[n] != NODE_NULL) {
            d = 0;
            while (d < depth + 1) { uart_putstr("  "); d = d + 1; }
            uart_puts("(OTHERWISE)");
            nd_dump(nd_ival[n], depth + 1);
        }
        nd_dump(nd_next[n], depth);
        return;
    }
    nd_dump(nd_next[n], depth);
}

#endif
