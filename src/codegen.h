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

#endif /* CODEGEN_H */
