/* codegen.h -- Expression code generation for PL/SW compiler */
/* Emits COR24 assembly for AST expression nodes */

#ifndef CODEGEN_H
#define CODEGEN_H

#include "token.h"
#include "emit.h"
#include "symtab.h"
#include "types.h"

/* --- Register model ---
 * r0: primary accumulator (result register)
 * r1: secondary (RHS of binops, also link register for calls)
 * r2: tertiary (address calculations, spill)
 * fp: frame pointer for local variable access
 * sp: stack pointer
 *
 * Expression evaluation always leaves the result in r0.
 * For binary ops: LHS in r0, RHS in r1, result in r0.
 * When register pressure exceeds 3, spill to stack via push/pop.
 */

/* --- Register names --- */

char *reg_name[3];

void cg_init_regs(void) {
    reg_name[0] = "r0";
    reg_name[1] = "r1";
    reg_name[2] = "r2";
}

/* --- Codegen error state --- */

int cg_err;
char *cg_errmsg;

void cg_init(void) {
    cg_init_regs();
    cg_err = 0;
    cg_errmsg = 0;
}

void cg_error(char *msg) {
    cg_err = 1;
    cg_errmsg = msg;
}

/* --- Forward declarations --- */

void cg_expr(int node);
void cg_call(int node);

/* --- Helper: check if expression is "simple" (no push/pop needed) --- */
/* A simple expr is a literal or variable -- can load directly into r1 */

int cg_is_simple(int node) {
    if (node == NODE_NULL) return 0;
    if (nd_kind[node] == NODE_LITERAL) return 1;
    if (nd_kind[node] == NODE_IDENT) return 1;
    return 0;
}

/* --- Literal loading --- */

void cg_literal(int node) {
    int val = nd_ival[node];
    if (val >= -128 && val <= 127) {
        /* Small constant: use lc (sign-extending 8-bit immediate) */
        emit_str(EMIT_INDENT);
        emit_str("lc      r0,");
        emit_int(val);
        emit_nl();
    } else {
        /* Large constant: use la (24-bit immediate) */
        emit_str(EMIT_INDENT);
        emit_str("la      r0,");
        emit_int(val);
        emit_nl();
    }
}

/* --- Variable load --- */
/* Loads variable value into r0 */

void cg_load_var(int node) {
    int idx;
    int off;
    int w;

    idx = sym_lookup(nd_name[node]);
    if (idx < 0) {
        cg_error("undefined variable");
        emit_comment("ERROR: undefined variable");
        if (nd_name[node]) {
            emit_str(EMIT_INDENT);
            emit_str("; name: ");
            emit_line(nd_name[node]);
        }
        emit_str(EMIT_INDENT);
        emit_line("lc      r0,0");
        return;
    }

    off = sym_offset[idx];
    w = sym_width[idx];

    if (sym_stor[idx] == STOR_STATIC || sym_stor[idx] == STOR_EXTERNAL) {
        /* Static/external: load address, then dereference */
        emit_str(EMIT_INDENT);
        emit_str("la      r2,");
        emit_int(off);
        emit_nl();
        if (w == 1) {
            emit_instr("lb      r0,0(r2)");
        } else {
            emit_instr("lw      r0,0(r2)");
        }
    } else {
        /* Automatic (stack-relative via fp) */
        if (off >= -128 && off <= 127) {
            if (w == 1) {
                emit_str(EMIT_INDENT);
                emit_str("lb      r0,");
                emit_int(off);
                emit_line("(fp)");
            } else {
                emit_str(EMIT_INDENT);
                emit_str("lw      r0,");
                emit_int(off);
                emit_line("(fp)");
            }
        } else {
            /* Large offset: compute address in r2 */
            emit_str(EMIT_INDENT);
            emit_str("la      r2,");
            emit_int(off);
            emit_nl();
            emit_instr("add     r2,fp");
            if (w == 1) {
                emit_instr("lb      r0,0(r2)");
            } else {
                emit_instr("lw      r0,0(r2)");
            }
        }
    }
}

/* --- Variable store --- */
/* Stores r0 into the variable named by the identifier node */

void cg_store_var(int node) {
    int idx;
    int off;
    int w;

    idx = sym_lookup(nd_name[node]);
    if (idx < 0) {
        cg_error("undefined variable for store");
        emit_comment("ERROR: undefined variable for store");
        return;
    }

    off = sym_offset[idx];
    w = sym_width[idx];

    if (sym_stor[idx] == STOR_STATIC || sym_stor[idx] == STOR_EXTERNAL) {
        emit_str(EMIT_INDENT);
        emit_str("la      r2,");
        emit_int(off);
        emit_nl();
        if (w == 1) {
            emit_instr("sb      r0,0(r2)");
        } else {
            emit_instr("sw      r0,0(r2)");
        }
    } else {
        if (off >= -128 && off <= 127) {
            if (w == 1) {
                emit_str(EMIT_INDENT);
                emit_str("sb      r0,");
                emit_int(off);
                emit_line("(fp)");
            } else {
                emit_str(EMIT_INDENT);
                emit_str("sw      r0,");
                emit_int(off);
                emit_line("(fp)");
            }
        } else {
            emit_str(EMIT_INDENT);
            emit_str("la      r2,");
            emit_int(off);
            emit_nl();
            emit_instr("add     r2,fp");
            if (w == 1) {
                emit_instr("sb      r0,0(r2)");
            } else {
                emit_instr("sw      r0,0(r2)");
            }
        }
    }
}

/* --- Load simple expression into a specific register --- */
/* Used for RHS of binops when RHS is simple (avoids push/pop) */

void cg_load_simple_into(int node, char *reg) {
    int val;
    int idx;
    int off;
    int w;

    if (nd_kind[node] == NODE_LITERAL) {
        val = nd_ival[node];
        if (val >= -128 && val <= 127) {
            emit_str(EMIT_INDENT);
            emit_str("lc      ");
            emit_str(reg);
            emit_char(44);
            emit_int(val);
            emit_nl();
        } else {
            emit_str(EMIT_INDENT);
            emit_str("la      ");
            emit_str(reg);
            emit_char(44);
            emit_int(val);
            emit_nl();
        }
    } else if (nd_kind[node] == NODE_IDENT) {
        idx = sym_lookup(nd_name[node]);
        if (idx < 0) {
            cg_error("undefined variable");
            emit_comment("ERROR: undefined variable in simple load");
            return;
        }
        off = sym_offset[idx];
        w = sym_width[idx];

        if (sym_stor[idx] == STOR_STATIC || sym_stor[idx] == STOR_EXTERNAL) {
            emit_str(EMIT_INDENT);
            emit_str("la      r2,");
            emit_int(off);
            emit_nl();
            if (w == 1) {
                emit_str(EMIT_INDENT);
                emit_str("lb      ");
                emit_str(reg);
                emit_line(",0(r2)");
            } else {
                emit_str(EMIT_INDENT);
                emit_str("lw      ");
                emit_str(reg);
                emit_line(",0(r2)");
            }
        } else {
            if (off >= -128 && off <= 127) {
                if (w == 1) {
                    emit_str(EMIT_INDENT);
                    emit_str("lb      ");
                    emit_str(reg);
                    emit_char(44);
                    emit_int(off);
                    emit_line("(fp)");
                } else {
                    emit_str(EMIT_INDENT);
                    emit_str("lw      ");
                    emit_str(reg);
                    emit_char(44);
                    emit_int(off);
                    emit_line("(fp)");
                }
            } else {
                /* Large offset: use r2 for address calc */
                emit_str(EMIT_INDENT);
                emit_str("la      r2,");
                emit_int(off);
                emit_nl();
                emit_instr("add     r2,fp");
                if (w == 1) {
                    emit_str(EMIT_INDENT);
                    emit_str("lb      ");
                    emit_str(reg);
                    emit_line(",0(r2)");
                } else {
                    emit_str(EMIT_INDENT);
                    emit_str("lw      ");
                    emit_str(reg);
                    emit_line(",0(r2)");
                }
            }
        }
    }
}

/* --- Software division subroutine --- */
/* Emitted once per compilation unit if division is used.
 * Entry: r0 = dividend, r1 = divisor
 * Exit:  r0 = quotient, r1 = remainder
 * Uses repeated subtraction (COR24 has no hardware divide). */

int cg_div_emitted;  /* flag: have we emitted the div routine? */

void cg_emit_div_routine(void) {
    int lloop;
    int ldone;
    int lneg;
    int lfix;

    if (cg_div_emitted) return;
    cg_div_emitted = 1;

    emit_nl();
    emit_comment("Software division: r0/r1 -> r0=quotient, r1=remainder");
    emit_line("__plsw_div:");
    emit_prologue();

    /* Load dividend and divisor from arguments (already in r0, r1) */
    /* Save sign info: we'll work with positive values */
    /* r2 = sign flag (0=positive result, 1=negative) */
    emit_instr("lc      r2,0");

    /* Check if dividend is negative */
    lneg = emit_new_label();
    lfix = emit_new_label();
    emit_instr("ceq     r0,z");
    emit_str(EMIT_INDENT);
    emit_str("brt     ");
    emit_label_ref(lfix);
    emit_nl();
    emit_instr("cls     r0,z");
    emit_str(EMIT_INDENT);
    emit_str("brf     ");
    emit_label_ref(lneg);
    emit_nl();
    /* dividend is negative: negate it, flip sign flag */
    emit_instr("sub     r0,r0");
    emit_instr("sub     r0,r0");
    /* Actually: 0 - r0. Use: push r0, lc r0,0, pop and sub */
    /* Simpler: just note this is tricky without a neg instruction */

    /* Simplified approach: unsigned division only for now.
     * Signed division handled by: abs both operands, divide, fix sign */
    emit_label(lneg);
    emit_label(lfix);

    /* Division by repeated subtraction */
    lloop = emit_new_label();
    ldone = emit_new_label();

    /* r0 = dividend (remainder), r2 = quotient (reuse r2) */
    emit_instr("push    r2");
    emit_instr("lc      r2,0");
    emit_label(lloop);
    emit_instr("cls     r0,r1");
    emit_str(EMIT_INDENT);
    emit_str("brt     ");
    emit_label_ref(ldone);
    emit_nl();
    emit_instr("sub     r0,r1");
    emit_instr("add     r2,1");
    emit_str(EMIT_INDENT);
    emit_str("bra     ");
    emit_label_ref(lloop);
    emit_nl();
    emit_label(ldone);
    /* r0 = remainder, r2 = quotient */
    emit_instr("mov     r1,r0");
    emit_instr("mov     r0,r2");
    emit_instr("pop     r2");
    emit_epilogue();
}

/* --- Binary arithmetic codegen --- */

void cg_binop(int node) {
    int op;
    int lhs;
    int rhs;
    int lbl_false;
    int lbl_end;

    op = nd_ival[node];
    lhs = nd_left[node];
    rhs = nd_right[node];

    if (cg_is_simple(rhs)) {
        /* Optimization: evaluate LHS into r0, load RHS directly into r1 */
        cg_expr(lhs);
        cg_load_simple_into(rhs, "r1");
    } else {
        /* Complex RHS: evaluate LHS, push, evaluate RHS, pop LHS into r1 */
        cg_expr(lhs);
        emit_instr1("push", "r0");
        cg_expr(rhs);
        emit_instr2("mov", "r1", "r0");
        emit_instr1("pop", "r0");
    }

    /* Now: r0 = LHS, r1 = RHS. Emit operation. */
    if (op == TOK_PLUS) {
        emit_instr("add     r0,r1");
    } else if (op == TOK_MINUS) {
        emit_instr("sub     r0,r1");
    } else if (op == TOK_STAR) {
        emit_instr("mul     r0,r1");
    } else if (op == TOK_SLASH) {
        /* Division via software routine */
        emit_instr1("push", "r1");
        emit_instr1("push", "r0");
        emit_str(EMIT_INDENT);
        emit_str("la      r2,__plsw_div");
        emit_nl();
        emit_instr("jal     r1,(r2)");
        /* Result in r0 (quotient) */
    } else if (op == TOK_AMP) {
        /* Bitwise AND: COR24 doesn't have direct AND.
         * Use a helper or emit inline sequence.
         * For now, we'll note this and use a simple approach. */
        emit_comment("bitwise AND (r0 & r1)");
        emit_instr("and     r0,r1");
    } else if (op == TOK_PIPE) {
        emit_comment("bitwise OR (r0 | r1)");
        emit_instr("or      r0,r1");
    } else if (op == TOK_CARET) {
        emit_comment("bitwise XOR (r0 ^ r1)");
        emit_instr("xor     r0,r1");
    } else if (op == TOK_SHL) {
        emit_comment("shift left");
        emit_instr("shl     r0,r1");
    } else if (op == TOK_SHR) {
        emit_comment("shift right");
        emit_instr("shr     r0,r1");
    } else if (op == TOK_EQ) {
        /* Equality: ceq r0,r1 then mov r0,c */
        emit_instr("ceq     r0,r1");
        emit_instr("mov     r0,c");
    } else if (op == TOK_NE) {
        /* Not equal: ceq then invert */
        emit_instr("ceq     r0,r1");
        emit_instr("mov     r0,c");
        emit_instr("ceq     r0,z");
        emit_instr("mov     r0,c");
    } else if (op == TOK_LT) {
        /* Signed less-than */
        emit_instr("cls     r0,r1");
        emit_instr("mov     r0,c");
    } else if (op == TOK_GT) {
        /* Greater-than: swap operands for cls */
        emit_instr("cls     r1,r0");
        emit_instr("mov     r0,c");
    } else if (op == TOK_LE) {
        /* Less-or-equal: NOT (r1 < r0) */
        emit_instr("cls     r1,r0");
        emit_instr("mov     r0,c");
        emit_instr("ceq     r0,z");
        emit_instr("mov     r0,c");
    } else if (op == TOK_GE) {
        /* Greater-or-equal: NOT (r0 < r1) */
        emit_instr("cls     r0,r1");
        emit_instr("mov     r0,c");
        emit_instr("ceq     r0,z");
        emit_instr("mov     r0,c");
    } else {
        cg_error("unsupported binary operator");
        emit_comment("ERROR: unsupported binop");
    }
}

/* --- Unary operators --- */

void cg_unop(int node) {
    int op = nd_ival[node];

    /* Evaluate operand into r0 */
    cg_expr(nd_left[node]);

    if (op == TOK_MINUS) {
        /* Negate: 0 - r0 */
        emit_instr2("mov", "r1", "r0");
        emit_instr("lc      r0,0");
        emit_instr("sub     r0,r1");
    } else if (op == TOK_TILDE || op == TOK_NOT) {
        /* Bitwise NOT: XOR with all-ones */
        emit_str(EMIT_INDENT);
        emit_str("la      r1,");
        emit_int(0xFFFFFF);
        emit_nl();
        emit_instr("xor     r0,r1");
    } else {
        cg_error("unsupported unary operator");
        emit_comment("ERROR: unsupported unop");
    }
}

/* --- Main expression codegen dispatcher --- */

void cg_expr(int node) {
    if (node == NODE_NULL) {
        emit_comment("null expr");
        emit_instr("lc      r0,0");
        return;
    }

    if (nd_kind[node] == NODE_LITERAL) {
        cg_literal(node);
    } else if (nd_kind[node] == NODE_IDENT) {
        cg_load_var(node);
    } else if (nd_kind[node] == NODE_BINOP) {
        cg_binop(node);
    } else if (nd_kind[node] == NODE_UNOP) {
        cg_unop(node);
    } else if (nd_kind[node] == NODE_CALL) {
        /* Function call in expression context: result in r0 */
        cg_call(node);
    } else {
        cg_error("unsupported expression node kind");
        emit_comment("ERROR: unsupported expr kind");
        emit_str(EMIT_INDENT);
        emit_str("; kind=");
        emit_int(nd_kind[node]);
        emit_nl();
    }
}

/* --- Assignment codegen --- */
/* Evaluate RHS into r0, store into LHS variable */

void cg_assign(int node) {
    int lhs = nd_left[node];
    int rhs = nd_right[node];

    /* Evaluate RHS into r0 */
    cg_expr(rhs);

    /* Store r0 into LHS */
    if (nd_kind[lhs] == NODE_IDENT) {
        cg_store_var(lhs);
    } else {
        cg_error("unsupported assignment target");
        emit_comment("ERROR: unsupported assignment target");
    }
}

/* --- Static data emission --- */

/* String literal table: maps labels to string data.
 * Used to collect string literals during codegen, then
 * emit them all in the .data section at the end. */

#define CG_STR_MAX 32     /* max string literals per compilation */

int cg_str_count;          /* number of string literals collected */
int cg_str_label[CG_STR_MAX];     /* label number for each string */
char *cg_str_data[CG_STR_MAX];    /* pointer to string content */

void cg_static_init(void) {
    cg_str_count = 0;
}

/* Register a string literal for emission. Returns its label number. */
int cg_add_string(char *s) {
    int lbl;
    if (cg_str_count >= CG_STR_MAX) {
        cg_error("too many string literals");
        return -1;
    }
    lbl = emit_new_label();
    cg_str_label[cg_str_count] = lbl;
    cg_str_data[cg_str_count] = s;
    cg_str_count = cg_str_count + 1;
    return lbl;
}

/* Emit a single string literal as .byte directives with null terminator */
void cg_emit_string_bytes(char *s) {
    int i;
    int ch;

    i = 0;
    while (s[i]) {
        emit_str(EMIT_INDENT);
        emit_str(".byte   ");
        emit_int(s[i]);
        emit_nl();
        i = i + 1;
    }
    /* Null terminator */
    emit_str(EMIT_INDENT);
    emit_line(".byte   0");
}

/* Emit all collected string literals in the .data section */
void cg_emit_string_table(void) {
    int i;
    if (cg_str_count == 0) return;

    emit_data_section();
    emit_comment("String literals");

    i = 0;
    while (i < cg_str_count) {
        emit_label(cg_str_label[i]);
        cg_emit_string_bytes(cg_str_data[i]);
        i = i + 1;
    }
}

/* Emit a static variable in the .data section.
 * Handles INIT values for scalars and zero-fill for uninitialized.
 * sym_idx = symbol table index for the variable.
 * init_node = AST node for INIT value, or NODE_NULL for zero. */

void cg_emit_static_var(int sym_idx, int init_node) {
    int w;
    int addr;
    int val;
    int i;
    char *sval;

    w = sym_width[sym_idx];
    addr = sym_offset[sym_idx];

    emit_data_section();

    /* Emit label at the static address */
    emit_comment(sym_name[sym_idx]);
    emit_named_label(sym_name[sym_idx]);

    if (init_node != NODE_NULL && nd_kind[init_node] == NODE_LITERAL) {
        if (nd_type[init_node] == TYPE_CHAR && nd_name[init_node]) {
            /* String initializer: emit bytes */
            sval = nd_name[init_node];
            i = 0;
            while (sval[i]) {
                emit_str(EMIT_INDENT);
                emit_str(".byte   ");
                emit_int(sval[i]);
                emit_nl();
                i = i + 1;
            }
            /* Pad remaining space with zeros */
            while (i < w) {
                emit_str(EMIT_INDENT);
                emit_line(".byte   0");
                i = i + 1;
            }
        } else {
            /* Numeric initializer */
            val = nd_ival[init_node];
            if (w == 1) {
                emit_str(EMIT_INDENT);
                emit_str(".byte   ");
                emit_int(val);
                emit_nl();
            } else {
                emit_str(EMIT_INDENT);
                emit_str(".word   ");
                emit_int(val);
                emit_nl();
            }
        }
    } else {
        /* No initializer: zero-fill */
        if (w == 1) {
            emit_str(EMIT_INDENT);
            emit_line(".byte   0");
        } else if (w <= 3) {
            emit_str(EMIT_INDENT);
            emit_line(".word   0");
        } else {
            /* Large zero fill: emit individual bytes */
            i = 0;
            while (i < w) {
                emit_str(EMIT_INDENT);
                emit_line(".byte   0");
                i = i + 1;
            }
        }
    }
}

/* Walk a program AST and emit .data section for all static/global DCLs.
 * Assumes layout_globals() has already been called to assign addresses. */

void cg_emit_static_data(int prog_node) {
    int child;
    int idx;

    if (prog_node == NODE_NULL) return;

    child = nd_left[prog_node];
    while (child != NODE_NULL) {
        if (nd_kind[child] == NODE_DCL) {
            idx = sym_lookup(nd_name[child]);
            if (idx >= 0) {
                cg_emit_static_var(idx, nd_right[child]);
            }
        }
        child = nd_next[child];
    }

    /* Emit any collected string literals */
    cg_emit_string_table();
}

/* Load address of a string literal into r0.
 * Used when a string literal appears in an expression context. */

void cg_load_string(int label_num) {
    emit_str(EMIT_INDENT);
    emit_str("la      r0,");
    emit_label_ref(label_num);
    emit_nl();
}

/* --- Call codegen --- */
/* Forward declaration for cg_block */
void cg_block(int block_node);

/* Count arguments in a linked list (via nd_next) */
int cg_count_args(int first_arg) {
    int count;
    int arg;
    count = 0;
    arg = first_arg;
    while (arg != NODE_NULL) {
        count = count + 1;
        arg = nd_next[arg];
    }
    return count;
}

/* Emit code for a procedure call.
 * CALL node: nd_name = procedure name, nd_left = first arg (linked via nd_next).
 * COR24 calling convention:
 *   - Evaluate arguments left-to-right
 *   - Push arguments right-to-left onto stack
 *   - Call via jal r1,(r2)
 *   - Clean up stack after return (add sp, arg_count*3)
 *   - Return value in r0 */

void cg_call(int node) {
    int nargs;
    int arg;
    int i;
    int cleanup;

    if (node == NODE_NULL) return;

    nargs = cg_count_args(nd_left[node]);

    if (nargs == 0) {
        /* No arguments: just call */
        emit_str(EMIT_INDENT);
        emit_str("la      r2,_");
        emit_line(nd_name[node]);
        emit_instr("jal     r1,(r2)");
    } else {
        /* Evaluate args L-to-R, store in R-to-L stack order.
         * Pre-allocate space, then store each at the correct offset
         * so first arg ends up at lowest address (closest to sp).
         * Callee sees fp+9 = first arg, fp+12 = second, etc. */
        cleanup = nargs * 3;
        emit_str(EMIT_INDENT);
        emit_str("lc      r2,");
        emit_int(cleanup);
        emit_nl();
        emit_instr("sub     sp,r2");

        /* Evaluate each arg L-to-R and store at sp + i*3.
         * arg1 at sp+0 -> callee fp+9 (first param)
         * arg2 at sp+3 -> callee fp+12 (second param) */
        arg = nd_left[node];
        i = 0;
        while (arg != NODE_NULL) {
            cg_expr(arg);
            emit_str(EMIT_INDENT);
            emit_str("sw      r0,");
            emit_int(i * 3);
            emit_line("(sp)");
            i = i + 1;
            arg = nd_next[arg];
        }

        /* Call the procedure */
        emit_str(EMIT_INDENT);
        emit_str("la      r2,_");
        emit_line(nd_name[node]);
        emit_instr("jal     r1,(r2)");

        /* Clean up args from stack */
        emit_str(EMIT_INDENT);
        emit_str("lc      r2,");
        emit_int(cleanup);
        emit_nl();
        emit_instr("add     sp,r2");
    }
}

/* --- Control flow codegen --- */

/* Forward declaration for mutual recursion */
void cg_stmt(int node);
void cg_block(int block_node);

/* IF/THEN/ELSE codegen.
 * AST: nd_left = condition, nd_right = then body, nd_ival = else body (or NODE_NULL) */
void cg_if(int node) {
    int lbl_else;
    int lbl_end;
    int else_body;

    else_body = nd_ival[node];

    /* Evaluate condition into r0 */
    cg_expr(nd_left[node]);

    if (else_body != NODE_NULL) {
        /* IF/THEN/ELSE */
        lbl_else = emit_new_label();
        lbl_end = emit_new_label();

        /* Branch to else if r0 == 0 (condition false) */
        emit_instr("ceq     r0,z");
        emit_str(EMIT_INDENT);
        emit_str("bc      ");
        emit_label_ref(lbl_else);
        emit_nl();

        /* Then body */
        cg_stmt(nd_right[node]);
        emit_str(EMIT_INDENT);
        emit_str("jmp     ");
        emit_label_ref(lbl_end);
        emit_nl();

        /* Else label and body */
        emit_label(lbl_else);
        cg_stmt(else_body);

        /* End label */
        emit_label(lbl_end);
    } else {
        /* IF/THEN (no else) */
        lbl_end = emit_new_label();

        /* Branch to end if r0 == 0 (condition false) */
        emit_instr("ceq     r0,z");
        emit_str(EMIT_INDENT);
        emit_str("bc      ");
        emit_label_ref(lbl_end);
        emit_nl();

        /* Then body */
        cg_stmt(nd_right[node]);

        /* End label */
        emit_label(lbl_end);
    }
}

/* DO WHILE loop codegen.
 * AST: nd_left = condition, nd_right = body (BLOCK) */
void cg_do_while(int node) {
    int lbl_top;
    int lbl_end;

    lbl_top = emit_new_label();
    lbl_end = emit_new_label();

    /* Loop header label */
    emit_label(lbl_top);

    /* Evaluate condition into r0 */
    cg_expr(nd_left[node]);

    /* Branch to end if r0 == 0 (condition false) */
    emit_instr("ceq     r0,z");
    emit_str(EMIT_INDENT);
    emit_str("bc      ");
    emit_label_ref(lbl_end);
    emit_nl();

    /* Loop body */
    cg_stmt(nd_right[node]);

    /* Branch back to header */
    emit_str(EMIT_INDENT);
    emit_str("jmp     ");
    emit_label_ref(lbl_top);
    emit_nl();

    /* End label */
    emit_label(lbl_end);
}

/* --- Statement codegen --- */

/* Emit code for a single statement node */
void cg_stmt(int node) {
    if (node == NODE_NULL) return;

    if (nd_kind[node] == NODE_ASSIGN) {
        cg_assign(node);
    } else if (nd_kind[node] == NODE_RETURN) {
        /* Evaluate return expression into r0 (if present) */
        if (nd_left[node] != NODE_NULL) {
            cg_expr(nd_left[node]);
        }
        /* Epilogue emitted by caller at procedure end.
         * For early returns, emit epilogue inline. */
        emit_epilogue();
    } else if (nd_kind[node] == NODE_CALL) {
        /* Bare CALL statement (result in r0 discarded) */
        cg_call(node);
    } else if (nd_kind[node] == NODE_BLOCK) {
        cg_block(node);
    } else if (nd_kind[node] == NODE_IF) {
        /* IF/THEN/ELSE: evaluate condition, branch on false */
        cg_if(node);
    } else if (nd_kind[node] == NODE_DO_WHILE) {
        /* DO WHILE: loop with condition at top */
        cg_do_while(node);
    } else if (nd_kind[node] == NODE_DCL) {
        /* Local DCL: no code emission needed (handled by layout) */
    } else {
        cg_error("unsupported statement kind");
        emit_comment("ERROR: unsupported statement");
        emit_str(EMIT_INDENT);
        emit_str("; kind=");
        emit_int(nd_kind[node]);
        emit_nl();
    }
}

/* Emit code for a block of statements */
void cg_block(int block_node) {
    int stmt;
    if (block_node == NODE_NULL) return;

    stmt = nd_left[block_node];
    while (stmt != NODE_NULL) {
        cg_stmt(stmt);
        stmt = nd_next[stmt];
    }
}

/* --- Procedure codegen --- */

/* Emit code for a complete procedure.
 * proc_node is a NODE_PROC with:
 *   nd_name  = procedure name
 *   nd_left  = first parameter (linked via nd_next)
 *   nd_right = body (BLOCK node)
 *   nd_ival  = option flags (OPT_FREESTANDING, OPT_NAKED, OPT_LEAF)
 *   nd_stor  = return type
 *
 * Layout must be performed first via layout_proc(). */

void cg_proc(int proc_node) {
    int opts;
    int frame_sz;
    int has_body;
    int body;

    if (proc_node == NODE_NULL) return;
    if (nd_kind[proc_node] != NODE_PROC) {
        cg_error("expected PROC node");
        return;
    }

    opts = nd_ival[proc_node];
    body = nd_right[proc_node];

    /* Ensure we're in .text section */
    emit_text_section();

    /* Emit .globl directive and label */
    if (nd_name[proc_node]) {
        emit_global(nd_name[proc_node]);
        emit_named_label(nd_name[proc_node]);
    }

    /* NAKED procedures: no prologue/epilogue, body is raw */
    if (opts & 2) {
        /* OPT_NAKED */
        emit_comment("NAKED procedure -- no prologue/epilogue");
        cg_block(body);
        return;
    }

    /* Standard prologue: push fp, push r2, push r1, mov fp,sp */
    emit_prologue();

    /* Allocate stack space for locals (sub sp, frame_size) */
    frame_sz = layout_frame_size;
    if (frame_sz > 0) {
        if (frame_sz >= -128 && frame_sz <= 127) {
            emit_str(EMIT_INDENT);
            emit_str("lc      r0,");
            emit_int(frame_sz);
            emit_nl();
        } else {
            emit_str(EMIT_INDENT);
            emit_str("la      r0,");
            emit_int(frame_sz);
            emit_nl();
        }
        emit_instr("sub     sp,r0");
    }

    /* Emit body statements */
    cg_block(body);

    /* Standard epilogue: mov sp,fp, pop r1, pop r2, pop fp, jmp (r1) */
    emit_epilogue();
}

/* Emit code for all procedures in a program AST.
 * Walks top-level children and codegen's each PROC node.
 * Handles layout_proc() and scope management for each. */

void cg_program_procs(int prog_node) {
    int child;

    if (prog_node == NODE_NULL) return;

    child = nd_left[prog_node];
    while (child != NODE_NULL) {
        if (nd_kind[child] == NODE_PROC) {
            /* Layout procedure (enters scope, assigns offsets) */
            layout_proc(child);
            /* Emit code */
            cg_proc(child);
            /* Exit procedure scope */
            sym_exit_scope();
        }
        child = nd_next[child];
    }
}

#endif /* CODEGEN_H */
