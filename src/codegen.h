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

/* Get the assembly-level name for a symbol (mangled or original) */
char *sym_label(int idx) {
    if (sym_asm_name[idx]) return sym_asm_name[idx];
    return sym_name[idx];
}

/* --- Codegen error state --- */

int cg_err;
char *cg_errmsg;

void cg_init(void) {
    cg_init_regs();
    cg_err = 0;
    cg_errmsg = 0;
    cg_listing = 1;  /* enabled by default */
    cg_last_line = 0;
}

void cg_error(char *msg) {
    if (!cg_err) {
        /* Only report first error */
        uart_putstr("CODEGEN ERROR: ");
        uart_puts(msg);
    }
    cg_err = 1;
    cg_errmsg = msg;
}

/* --- Source listing state --- */

int cg_listing;        /* 1 = emit source line comments */
int cg_last_line;      /* last source line emitted as comment */

/* Emit a source line comment if the line changed */
void cg_source_comment(int node) {
    int ln;
    char buf[120];
    if (!cg_listing) return;
    if (node == NODE_NULL) return;
    ln = nd_line[node];
    if (ln <= 0 || ln == cg_last_line) return;
    cg_last_line = ln;
    src_line_text(ln - 1, buf, 120);
    if (buf[0]) {
        emit_str("; ");
        emit_int(ln);
        emit_str(": ");
        emit_line(buf);
    }
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
    /* SIZEOF(name): resolve to symbol width */
    if (nd_stor[node] == TOK_SIZEOF && nd_name[node]) {
        int si = sym_lookup(nd_name[node]);
        if (si >= 0) {
            val = sym_width[si];
        } else {
            cg_error("undefined name in SIZEOF");
            val = 0;
        }
    }
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
        /* Static/external: load address via label, then dereference */
        emit_str(EMIT_INDENT);
        emit_str("la      r2,_");
        emit_str(sym_label(idx));
        emit_nl();
        if (w == 1) {
            emit_instr("lbu     r0,0(r2)");
        } else {
            emit_instr("lw      r0,0(r2)");
        }
    } else {
        /* Automatic (stack-relative via fp) */
        if (off >= -128 && off <= 127) {
            if (w == 1) {
                emit_str(EMIT_INDENT);
                emit_str("lbu     r0,");
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
                emit_instr("lbu     r0,0(r2)");
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
        emit_str("la      r2,_");
        emit_str(sym_label(idx));
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
        /* SIZEOF(name): resolve to symbol width at codegen time */
        if (nd_stor[node] == TOK_SIZEOF && nd_name[node]) {
            idx = sym_lookup(nd_name[node]);
            if (idx >= 0) {
                val = sym_width[idx];
            } else {
                cg_error("undefined name in SIZEOF");
                emit_comment("ERROR: undefined name in SIZEOF");
                val = 0;
            }
        }
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
            emit_str("la      r2,_");
            emit_str(sym_label(idx));
            emit_nl();
            if (w == 1) {
                emit_str(EMIT_INDENT);
                emit_str("lbu     ");
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
                    emit_str("lbu     ");
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
                    emit_str("lbu     ");
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

    if (cg_div_emitted) return;
    cg_div_emitted = 1;

    emit_nl();
    emit_comment("Software division: args on stack, r0=quotient on return");
    emit_line("__plsw_div:");
    emit_prologue();

    /* Load dividend and divisor from stack args (COR24 calling convention).
     * Prologue pushes fp, r2, r1 (3 regs x 3 bytes = 9).
     * fp+9 = first arg (dividend), fp+12 = second arg (divisor). */
    emit_instr("lw      r0,9(fp)");
    emit_instr("lw      r1,12(fp)");

    /* Division by repeated subtraction: r0 = dividend, r1 = divisor */
    lloop = emit_new_label();
    ldone = emit_new_label();

    /* r2 = quotient counter */
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
    /* r0 = quotient */
    emit_instr("mov     r0,r2");
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
        /* Division via software routine (standard calling convention) */
        emit_instr1("push", "r1");
        emit_instr1("push", "r0");
        emit_str(EMIT_INDENT);
        emit_str("la      r2,__plsw_div");
        emit_nl();
        emit_instr("jal     r1,(r2)");
        emit_instr("add     sp,6");
        /* Result in r0 (quotient) */
    } else if (op == TOK_AMP || op == TOK_AND) {
        /* Bitwise/logical AND */
        emit_comment("AND (r0 & r1)");
        emit_instr("and     r0,r1");
    } else if (op == TOK_PIPE || op == TOK_OR) {
        emit_comment("OR (r0 | r1)");
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

/* --- Record field access codegen --- */

/* Compute the address of a record field into r2.
 * For NODE_FIELD_ACCESS: nd_left = record expression (must be NODE_IDENT),
 *   nd_name = field name.
 * Returns the field width (1 or 3) for load/store sizing, or 0 on error. */

int cg_field_addr(int node) {
    int rec_node;
    int sym_idx;
    int desc;
    int fidx;
    int foff;
    int fw;
    int rec_off;

    rec_node = nd_left[node];

    /* Currently only support simple record identifiers (REC.FIELD) */
    if (nd_kind[rec_node] != NODE_IDENT) {
        cg_error("field access requires record variable");
        emit_comment("ERROR: field access on non-ident");
        return 0;
    }

    /* Look up the record symbol */
    sym_idx = sym_lookup(nd_name[rec_node]);
    if (sym_idx < 0) {
        cg_error("undefined record variable");
        emit_comment("ERROR: undefined record variable");
        return 0;
    }

    /* Verify it's a record */
    if (!(sym_flags[sym_idx] & SYM_F_RECORD)) {
        cg_error("field access on non-record variable");
        emit_comment("ERROR: not a record");
        return 0;
    }

    /* Get the type descriptor */
    desc = sym_tdesc[sym_idx];
    if (desc < 0) {
        cg_error("record has no type descriptor");
        emit_comment("ERROR: no type descriptor");
        return 0;
    }

    /* Look up the field */
    fidx = td_field_lookup(desc, nd_name[node]);
    if (fidx < 0) {
        cg_error("undefined field in record");
        emit_comment("ERROR: undefined field");
        if (nd_name[node]) {
            emit_str(EMIT_INDENT);
            emit_str("; field: ");
            emit_line(nd_name[node]);
        }
        return 0;
    }

    foff = td_field_offset(desc, fidx);
    fw = td_field_width(desc, fidx);
    rec_off = sym_offset[sym_idx];

    /* Compute address: record_base + field_offset into r2 */
    if (sym_stor[sym_idx] == STOR_STATIC || sym_stor[sym_idx] == STOR_EXTERNAL) {
        /* Static: la r2, _VARNAME + field_offset */
        emit_str(EMIT_INDENT);
        emit_str("la      r2,_");
        emit_str(sym_label(sym_idx));
        if (foff > 0) {
            emit_char(43); /* '+' */
            emit_int(foff);
        }
        emit_nl();
    } else {
        /* Automatic: fp + record_offset + field_offset */
        int total_off = rec_off + foff;
        if (total_off >= -128 && total_off <= 127) {
            emit_str(EMIT_INDENT);
            emit_str("la      r2,");
            emit_int(total_off);
            emit_nl();
            emit_instr("add     r2,fp");
        } else {
            emit_str(EMIT_INDENT);
            emit_str("la      r2,");
            emit_int(total_off);
            emit_nl();
            emit_instr("add     r2,fp");
        }
    }

    return fw;
}

/* Load a record field value into r0 */
void cg_field_load(int node) {
    int fw;
    fw = cg_field_addr(node);
    if (fw == 0) return;

    if (fw == 1) {
        emit_instr("lbu     r0,0(r2)");
    } else {
        emit_instr("lw      r0,0(r2)");
    }
}

/* Store r0 into a record field.
 * Assumes r0 holds the value to store.
 * Saves r0 on stack while computing address, then restores and stores. */
void cg_field_store(int node) {
    int fw;

    /* Save the value while we compute the address */
    emit_instr("push    r0");
    fw = cg_field_addr(node);
    emit_instr("pop     r0");

    if (fw == 0) return;

    if (fw == 1) {
        emit_instr("sb      r0,0(r2)");
    } else {
        emit_instr("sw      r0,0(r2)");
    }
}

/* --- Array element access codegen --- */

/* Compute the address of an array element into r2.
 * For NODE_ARRAY_ACCESS: nd_name = array name, nd_left = index expression.
 * Returns the element width (1 or 3) for load/store sizing, or 0 on error. */

int cg_array_addr(int node) {
    int sym_idx;
    int elem_w;
    int arr_off;

    sym_idx = sym_lookup(nd_name[node]);
    if (sym_idx < 0) {
        cg_error("undefined array variable");
        emit_comment("ERROR: undefined array variable");
        return 0;
    }

    if (!(sym_flags[sym_idx] & SYM_F_ARRAY)) {
        cg_error("subscript on non-array variable");
        emit_comment("ERROR: not an array");
        return 0;
    }

    /* Element width from the base type */
    elem_w = type_width(sym_type[sym_idx]);
    if (elem_w == 0) elem_w = 3;

    arr_off = sym_offset[sym_idx];

    /* Evaluate index expression into r0 */
    cg_expr(nd_left[node]);

    /* Compute element offset: r0 = index * elem_w */
    if (elem_w == 3) {
        /* Multiply index by 3: r0 = r0 + r0 + r0 */
        emit_instr("mov     r1,r0");
        emit_instr("add     r0,r1");
        emit_instr("add     r0,r1");
    }
    /* elem_w == 1: no multiply needed, offset = index */
    /* elem_w == 2 would need shl by 1, but not common */

    /* r0 = element byte offset. Compute base + offset into r2. */
    emit_instr("mov     r2,r0");

    if (sym_stor[sym_idx] == STOR_STATIC || sym_stor[sym_idx] == STOR_EXTERNAL) {
        /* Static: r2 = r2 + label address */
        emit_str(EMIT_INDENT);
        emit_str("la      r0,_");
        emit_str(sym_label(sym_idx));
        emit_nl();
        emit_instr("add     r2,r0");
    } else {
        /* Automatic: r2 = r2 + fp + stack_offset */
        if (arr_off >= -128 && arr_off <= 127) {
            emit_str(EMIT_INDENT);
            emit_str("lc      r0,");
            emit_int(arr_off);
            emit_nl();
        } else {
            emit_str(EMIT_INDENT);
            emit_str("la      r0,");
            emit_int(arr_off);
            emit_nl();
        }
        emit_instr("add     r2,r0");
        emit_instr("add     r2,fp");
    }

    return elem_w;
}

/* Load an array element value into r0 */
void cg_array_load(int node) {
    int ew;
    ew = cg_array_addr(node);
    if (ew == 0) return;

    if (ew == 1) {
        emit_instr("lbu     r0,0(r2)");
    } else {
        emit_instr("lw      r0,0(r2)");
    }
}

/* Store r0 into an array element.
 * Saves r0 on stack while computing address, then restores and stores. */
void cg_array_store(int node) {
    int ew;

    emit_instr("push    r0");
    ew = cg_array_addr(node);
    emit_instr("pop     r0");

    if (ew == 0) return;

    if (ew == 1) {
        emit_instr("sb      r0,0(r2)");
    } else {
        emit_instr("sw      r0,0(r2)");
    }
}

/* --- ADDR (address-of) codegen --- */

/* Compute the address of a variable into r0.
 * For NODE_ADDR: nd_left = operand (NODE_IDENT, NODE_FIELD_ACCESS, or NODE_ARRAY_ACCESS).
 * Stack variables: fp + offset. Static: absolute address. */

void cg_addr(int node) {
    int operand;
    int idx;
    int off;

    operand = nd_left[node];
    if (operand == NODE_NULL) {
        cg_error("ADDR with no operand");
        emit_comment("ERROR: ADDR with no operand");
        emit_instr("lc      r0,0");
        return;
    }

    if (nd_kind[operand] == NODE_IDENT) {
        idx = sym_lookup(nd_name[operand]);
        if (idx < 0) {
            cg_error("undefined variable in ADDR");
            emit_comment("ERROR: undefined variable in ADDR");
            emit_instr("lc      r0,0");
            return;
        }
        off = sym_offset[idx];
        if (sym_stor[idx] == STOR_STATIC || sym_stor[idx] == STOR_EXTERNAL) {
            emit_str(EMIT_INDENT);
            emit_str("la      r0,_");
            emit_str(sym_label(idx));
            emit_nl();
        } else {
            /* Automatic: address = fp + offset */
            if (off >= -128 && off <= 127) {
                emit_str(EMIT_INDENT);
                emit_str("lc      r0,");
                emit_int(off);
                emit_nl();
            } else {
                emit_str(EMIT_INDENT);
                emit_str("la      r0,");
                emit_int(off);
                emit_nl();
            }
            emit_instr("add     r0,fp");
        }
    } else if (nd_kind[operand] == NODE_FIELD_ACCESS) {
        /* ADDR(rec.field) -- compute field address into r2, move to r0 */
        cg_field_addr(operand);
        emit_instr("mov     r0,r2");
    } else if (nd_kind[operand] == NODE_ARRAY_ACCESS) {
        /* ADDR(arr(i)) -- compute element address into r2, move to r0 */
        cg_array_addr(operand);
        emit_instr("mov     r0,r2");
    } else {
        cg_error("ADDR requires a variable, field, or array element");
        emit_comment("ERROR: unsupported ADDR operand");
        emit_instr("lc      r0,0");
    }
}

/* --- Pointer dereference codegen (ptr->field) --- */

/* Compute the address of a dereferenced field into r2.
 * For NODE_DEREF: nd_left = pointer expression, nd_name = field name.
 * Evaluates pointer into r0, looks up field offset in the pointed-to
 * record's type descriptor, adds offset, result address in r2.
 * Returns field width for load/store sizing, or 0 on error. */

int cg_deref_addr(int node) {
    int ptr_node;
    int sym_idx;
    int desc;
    int fidx;
    int foff;
    int fw;

    ptr_node = nd_left[node];

    /* Evaluate pointer expression into r0 */
    cg_expr(ptr_node);

    /* Look up the pointer symbol to find the pointed-to record type */
    if (nd_kind[ptr_node] != NODE_IDENT) {
        cg_error("dereference requires pointer variable");
        emit_comment("ERROR: deref on non-ident");
        return 0;
    }

    sym_idx = sym_lookup(nd_name[ptr_node]);
    if (sym_idx < 0) {
        cg_error("undefined pointer variable");
        emit_comment("ERROR: undefined pointer variable");
        return 0;
    }

    if (sym_type[sym_idx] != TYPE_PTR) {
        cg_error("dereference requires PTR type");
        emit_comment("ERROR: not a PTR");
        return 0;
    }

    /* Get the type descriptor for the pointed-to record */
    desc = sym_tdesc[sym_idx];

    /* Look up the field -- try associated descriptor first,
       then search all BASED record descriptors */
    fidx = -1;
    if (desc >= 0) {
        fidx = td_field_lookup(desc, nd_name[node]);
    }
    if (fidx < 0) {
        /* Search all record descriptors for this field */
        fidx = td_field_search(nd_name[node], &desc);
    }
    if (fidx < 0) {
        cg_error("undefined field in pointer dereference");
        emit_comment("ERROR: undefined field in deref");
        if (nd_name[node]) {
            emit_str(EMIT_INDENT);
            emit_str("; field: ");
            emit_line(nd_name[node]);
        }
        return 0;
    }

    foff = td_field_offset(desc, fidx);
    fw = td_field_width(desc, fidx);

    /* r0 = pointer value (base address). Add field offset. */
    emit_instr("mov     r2,r0");
    if (foff > 0) {
        if (foff >= -128 && foff <= 127) {
            emit_str(EMIT_INDENT);
            emit_str("lc      r0,");
            emit_int(foff);
            emit_nl();
        } else {
            emit_str(EMIT_INDENT);
            emit_str("la      r0,");
            emit_int(foff);
            emit_nl();
        }
        emit_instr("add     r2,r0");
    }

    return fw;
}

/* Load a value through pointer dereference into r0 */
void cg_deref_load(int node) {
    int fw;
    fw = cg_deref_addr(node);
    if (fw == 0) return;

    if (fw == 1) {
        emit_instr("lbu     r0,0(r2)");
    } else {
        emit_instr("lw      r0,0(r2)");
    }
}

/* Store r0 into a dereferenced field.
 * Saves r0 on stack while computing address, then restores and stores. */
void cg_deref_store(int node) {
    int fw;

    emit_instr("push    r0");
    fw = cg_deref_addr(node);
    emit_instr("pop     r0");

    if (fw == 0) return;

    if (fw == 1) {
        emit_instr("sb      r0,0(r2)");
    } else {
        emit_instr("sw      r0,0(r2)");
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
    } else if (nd_kind[node] == NODE_ARRAY_ACCESS) {
        /* Array element access: arr(i) */
        cg_array_load(node);
    } else if (nd_kind[node] == NODE_CALL) {
        /* Check if this is actually an array access (parsed as CALL) */
        if (nd_name[node]) {
            int _si = sym_lookup(nd_name[node]);
            if (_si >= 0 && (sym_flags[_si] & SYM_F_ARRAY)) {
                cg_array_load(node);
            } else {
                cg_call(node);
            }
        } else {
            cg_call(node);
        }
    } else if (nd_kind[node] == NODE_FIELD_ACCESS) {
        /* Record field access: rec.field */
        cg_field_load(node);
    } else if (nd_kind[node] == NODE_ADDR) {
        /* ADDR(var) -- address-of */
        cg_addr(node);
    } else if (nd_kind[node] == NODE_DEREF) {
        /* ptr->field -- pointer dereference */
        cg_deref_load(node);
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

/* Emit unrolled byte stores for string literal to ptr->field.
 * r2 = base address of field (already computed by cg_deref_addr).
 * s = string content, len = bytes to copy. */
void cg_string_to_deref(int deref_node, char *s, int len) {
    int fw;
    int i;

    /* Compute field address into r2 */
    fw = cg_deref_addr(deref_node);
    if (fw == 0) return;

    emit_comment("string -> ptr->field");
    i = 0;
    while (i < len && s[i]) {
        emit_str(EMIT_INDENT);
        emit_str("lc      r0,");
        emit_int(s[i]);
        emit_nl();
        emit_str(EMIT_INDENT);
        emit_str("sb      r0,");
        emit_int(i);
        emit_str("(r2)");
        emit_nl();
        i = i + 1;
    }
    /* Null-terminate if room */
    if (i < fw) {
        emit_str(EMIT_INDENT);
        emit_str("lc      r0,0");
        emit_nl();
        emit_str(EMIT_INDENT);
        emit_str("sb      r0,");
        emit_int(i);
        emit_str("(r2)");
        emit_nl();
    }
}

void cg_assign(int node) {
    int lhs = nd_left[node];
    int rhs = nd_right[node];

    /* Special case: string literal assigned to ptr->field (CHAR array) */
    if (nd_kind[lhs] == NODE_DEREF
        && nd_kind[rhs] == NODE_LITERAL
        && nd_type[rhs] == TYPE_CHAR
        && nd_name[rhs]) {
        cg_string_to_deref(lhs, nd_name[rhs], str_len(nd_name[rhs]));
        return;
    }

    /* Evaluate RHS into r0 */
    cg_expr(rhs);

    /* Store r0 into LHS */
    if (nd_kind[lhs] == NODE_IDENT) {
        cg_store_var(lhs);
    } else if (nd_kind[lhs] == NODE_FIELD_ACCESS) {
        cg_field_store(lhs);
    } else if (nd_kind[lhs] == NODE_ARRAY_ACCESS) {
        cg_array_store(lhs);
    } else if (nd_kind[lhs] == NODE_DEREF) {
        cg_deref_store(lhs);
    } else {
        cg_error("unsupported assignment target");
        emit_comment("ERROR: unsupported assignment target");
    }
}

/* --- Static data emission --- */

/* String literal table: maps labels to string data.
 * Used to collect string literals during codegen, then
 * emit them all in the .data section at the end. */

#define CG_STR_MAX 128    /* max string literals per compilation */

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

/* Emit a string literal as comma-separated .byte with null terminator */
void cg_emit_string_bytes(char *s) {
    int i;

    emit_str(EMIT_INDENT);
    emit_str(".byte   ");
    i = 0;
    while (s[i]) {
        if (i > 0) emit_str(",");
        emit_int(s[i]);
        i = i + 1;
    }
    /* Null terminator */
    if (i > 0) emit_str(",");
    emit_str("0");
    emit_nl();
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

    /* Zero-fill without initializer: emit explicit .byte 0 entries
       (.comm overlaps with .data on COR24 assembler) */
    if (init_node == NODE_NULL && w > 3) {
        emit_comment(sym_label(sym_idx));
        emit_named_label(sym_label(sym_idx));
        emit_str(EMIT_INDENT);
        emit_str(".byte   ");
        i = 0;
        while (i < w) {
            if (i > 0) emit_str(",");
            emit_str("0");
            i = i + 1;
        }
        emit_nl();
        return;
    }

    /* Emit label at the static address */
    emit_comment(sym_label(sym_idx));
    emit_named_label(sym_label(sym_idx));

    if (init_node != NODE_NULL && nd_kind[init_node] == NODE_LITERAL) {
        if (nd_type[init_node] == TYPE_CHAR && nd_name[init_node]) {
            /* String initializer: emit as comma-separated .byte */
            sval = nd_name[init_node];
            emit_str(EMIT_INDENT);
            emit_str(".byte   ");
            i = 0;
            while (sval[i]) {
                if (i > 0) emit_str(",");
                emit_int(sval[i]);
                i = i + 1;
            }
            /* Pad remaining with zeros up to declared width */
            while (i < w) {
                emit_str(",0");
                i = i + 1;
            }
            emit_nl();
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
        /* No initializer: zero-fill (small vars only, large handled above) */
        if (w == 1) {
            emit_str(EMIT_INDENT);
            emit_line(".byte   0");
        } else {
            emit_str(EMIT_INDENT);
            emit_line(".word   0");
        }
    }
}

/* Emit .data for STATIC DCLs inside a procedure body.
 * Walks the block recursively to find NODE_DCL with STOR_STATIC. */
void cg_emit_proc_static_data(int body_node) {
    int stmt;
    int idx;

    if (body_node == NODE_NULL) return;

    /* Handle non-block statement nodes */
    if (nd_kind[body_node] == NODE_IF) {
        while (body_node != NODE_NULL && nd_kind[body_node] == NODE_IF) {
            cg_emit_proc_static_data(nd_right[body_node]);
            if (nd_ival[body_node] != NODE_NULL && nd_kind[nd_ival[body_node]] == NODE_IF) {
                body_node = nd_ival[body_node];
            } else {
                cg_emit_proc_static_data(nd_ival[body_node]);
                break;
            }
        }
        return;
    }
    if (nd_kind[body_node] == NODE_DO_WHILE || nd_kind[body_node] == NODE_DO_COUNT) {
        cg_emit_proc_static_data(nd_right[body_node]);
        return;
    }
    if (nd_kind[body_node] == NODE_SELECT) {
        int wh = nd_left[body_node];
        while (wh != NODE_NULL) {
            cg_emit_proc_static_data(nd_right[wh]);
            wh = nd_next[wh];
        }
        cg_emit_proc_static_data(nd_ival[body_node]);
        return;
    }

    /* Walk block children */
    stmt = nd_left[body_node];
    while (stmt != NODE_NULL) {
        if (nd_kind[stmt] == NODE_DCL && nd_stor[stmt] == STOR_STATIC) {
            idx = sym_lookup(nd_name[stmt]);
            if (idx >= 0) {
                cg_emit_static_var(idx, nd_right[stmt]);
            }
        } else if (nd_kind[stmt] == NODE_BLOCK) {
            cg_emit_proc_static_data(stmt);
        } else if (nd_kind[stmt] == NODE_IF) {
            cg_emit_proc_static_data(stmt);
        } else if (nd_kind[stmt] == NODE_DO_WHILE || nd_kind[stmt] == NODE_DO_COUNT) {
            cg_emit_proc_static_data(stmt);
        } else if (nd_kind[stmt] == NODE_SELECT) {
            cg_emit_proc_static_data(stmt);
        }
        stmt = nd_next[stmt];
    }
}

/* Walk a program AST and emit .data section for all static/global DCLs.
 * Assumes layout_globals() has already been called to assign addresses. */

void cg_emit_static_data(int prog_node) {
    int child;
    int idx;

    if (prog_node == NODE_NULL) return;

    /* Reset listing state so DCL lines show in data section */
    cg_last_line = 0;

    child = nd_left[prog_node];
    while (child != NODE_NULL) {
        if (nd_kind[child] == NODE_DCL) {
            /* In LIBRARY mode, suppress ALL top-level DCL data emission.
             * Top-level globals (BASED records, shared globals from
             * .msw includes, and any other DCLs) are owned by the main
             * module. Library modules reference them as externals
             * resolved by the linker via FIXUP.
             *
             * Library modules must use procedure-local STATIC variables
             * for module-private persistent state, not top-level DCLs. */
            if (def_defined("LIBRARY")) {
                child = nd_next[child];
                continue;
            }
            idx = sym_lookup(nd_name[child]);
            cg_source_comment(child);
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
 *   - Push arguments right-to-left onto stack (last arg pushed first)
 *   - Call via jal r1,(r2)
 *   - Clean up stack after return (add sp, arg_count*3)
 *   - Return value in r0
 *
 * COR24 constraint: sp-relative sw is not supported,
 * so we use push for each arg in reverse order. */

#define CG_MAX_ARGS 8

void cg_call(int node) {
    int nargs;
    int arg;
    int i;
    int cleanup;
    int arg_nodes[CG_MAX_ARGS];

    if (node == NODE_NULL) return;

    nargs = cg_count_args(nd_left[node]);

    if (nargs == 0) {
        /* No arguments: just call */
        emit_str(EMIT_INDENT);
        emit_str("la      r2,_");
        emit_line(nd_name[node]);
        emit_instr("jal     r1,(r2)");
    } else {
        /* Collect arg nodes into array for reverse-order push */
        arg = nd_left[node];
        i = 0;
        while (arg != NODE_NULL && i < CG_MAX_ARGS) {
            arg_nodes[i] = arg;
            i = i + 1;
            arg = nd_next[arg];
        }

        /* Push args R-to-L: evaluate last arg first, push it */
        i = nargs - 1;
        while (i >= 0) {
            cg_expr(arg_nodes[i]);
            emit_instr("push    r0");
            i = i - 1;
        }

        /* Call the procedure */
        emit_str(EMIT_INDENT);
        emit_str("la      r2,_");
        emit_line(nd_name[node]);
        emit_instr("jal     r1,(r2)");

        /* Clean up args from stack */
        cleanup = nargs * 3;
        emit_str(EMIT_INDENT);
        emit_str("add     sp,");
        emit_int(cleanup);
        emit_nl();
    }
}

/* --- Control flow codegen --- */

/* Forward declaration for mutual recursion */
void cg_stmt(int node);
void cg_block(int block_node);

/* IF/THEN/ELSE codegen -- iterative for ELSE IF chains (#33).
 * AST: nd_left = condition, nd_right = then body, nd_ival = else body (or NODE_NULL) */
void cg_if(int node) {
    int lbl_else;
    int lbl_end;
    int else_body;

    lbl_end = emit_new_label();

    while (1) {
        else_body = nd_ival[node];

        /* Evaluate condition into r0 */
        cg_expr(nd_left[node]);

        if (else_body != NODE_NULL) {
            /* IF/THEN/ELSE */
            lbl_else = emit_new_label();

            /* Branch to else if r0 == 0 (condition false) */
            emit_instr("ceq     r0,z");
            emit_branch_true(lbl_else);

            /* Then body */
            cg_stmt(nd_right[node]);
            emit_branch(lbl_end);

            /* Else label */
            emit_label(lbl_else);

            /* ELSE IF chain: iterate instead of recursing */
            if (nd_kind[else_body] == NODE_IF) {
                node = else_body;
                continue;
            }

            /* Plain ELSE body */
            cg_stmt(else_body);
        } else {
            /* IF/THEN (no else) */

            /* Branch to end if r0 == 0 (condition false) */
            emit_instr("ceq     r0,z");
            emit_branch_true(lbl_end);

            /* Then body */
            cg_stmt(nd_right[node]);
        }
        break;
    }

    /* End label */
    emit_label(lbl_end);
}

/* SELECT/WHEN/OTHERWISE codegen.
 * AST: nd_left = first WHEN child (sibling chain via nd_next),
 *       each WHEN: nd_left = condition, nd_right = body,
 *       nd_ival = OTHERWISE body (or NODE_NULL) */
void cg_select(int node) {
    int lbl_end;
    int when;

    lbl_end = emit_new_label();
    when = nd_left[node];

    while (when != NODE_NULL) {
        int lbl_next = emit_new_label();

        /* Evaluate condition into r0 */
        cg_expr(nd_left[when]);

        /* Branch to next WHEN if r0 == 0 (condition false) */
        emit_instr("ceq     r0,z");
        emit_branch_true(lbl_next);

        /* WHEN body */
        cg_stmt(nd_right[when]);
        emit_branch(lbl_end);

        /* Next WHEN label */
        emit_label(lbl_next);

        when = nd_next[when];
    }

    /* OTHERWISE body */
    if (nd_ival[node] != NODE_NULL) {
        cg_stmt(nd_ival[node]);
    }

    emit_label(lbl_end);
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
    emit_branch_true(lbl_end);

    /* Loop body */
    cg_stmt(nd_right[node]);

    /* Branch back to header */
    emit_branch(lbl_top);

    /* End label */
    emit_label(lbl_end);
}

/* DO I = start TO end; ... END; codegen.
 * Lowers to: I = start; WHILE (I <= end) { body; I = I + 1; }
 * AST: nd_name = loop var, nd_left = start, nd_ival = end expr, nd_right = body */
void cg_do_count(int node) {
    int lbl_top;
    int lbl_end;

    lbl_top = emit_new_label();
    lbl_end = emit_new_label();

    /* I = start: evaluate start expr into r0, store to loop var */
    cg_expr(nd_left[node]);
    cg_store_var(node);  /* nd_name[node] = loop var name */

    /* Loop header label */
    emit_label(lbl_top);

    /* Compare: load I into r0, save, eval end into r0, then compare */
    cg_load_var(node);       /* r0 = I */
    emit_instr("push    r0");
    cg_expr(nd_ival[node]);  /* r0 = end */
    emit_instr("mov     r1,r0");  /* r1 = end */
    emit_instr("pop     r0");     /* r0 = I */

    /* Branch to end if I > end (i.e., end < I) */
    emit_instr("cls     r1,r0");  /* carry if end < I */
    emit_branch_true(lbl_end);

    /* Loop body */
    cg_stmt(nd_right[node]);

    /* Increment: I = I + 1 */
    cg_load_var(node);
    emit_instr("lc      r1,1");
    emit_instr("add     r0,r1");
    cg_store_var(node);

    /* Branch back to header */
    emit_branch(lbl_top);

    /* End label */
    emit_label(lbl_end);
}

/* --- Inline assembly codegen --- */

/* Emit code for an ASM_BLOCK node.
 * Each child is a LITERAL node with nd_name holding the asm string.
 * Strings are emitted verbatim as instructions. */
void cg_asm_block(int node) {
    int child;
    emit_comment("ASM DO block");
    child = nd_left[node];
    while (child != NODE_NULL) {
        if (nd_name[child]) {
            emit_str(EMIT_INDENT);
            emit_line(nd_name[child]);
        }
        child = nd_next[child];
    }
}

/* --- Statement codegen --- */

/* Emit code for a single statement node */
void cg_stmt(int node) {
    if (node == NODE_NULL) return;

    /* Emit source line comment if listing enabled */
    cg_source_comment(node);

    if (nd_kind[node] == NODE_ASSIGN) {
        cg_assign(node);
    } else if (nd_kind[node] == NODE_SELECT) {
        cg_select(node);
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
    } else if (nd_kind[node] == NODE_DO_COUNT) {
        /* DO I = start TO end: counted loop */
        cg_do_count(node);
    } else if (nd_kind[node] == NODE_SELECT) {
        /* SELECT/WHEN/OTHERWISE: linear compare-and-branch chain */
        cg_select(node);
    } else if (nd_kind[node] == NODE_DCL) {
        /* Local DCL: no code emission needed (handled by layout) */
    } else if (nd_kind[node] == NODE_ASM_BLOCK) {
        cg_asm_block(node);
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

    /* Emit source line comment for PROC */
    cg_source_comment(proc_node);

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

    /* Allocate stack space for locals.
     * `add sp,i8` is signed 8-bit (-128..127); for frames larger
     * than 127 bytes the immediate would silently wrap (e.g. -204
     * encodes to +52, growing sp instead of shrinking it and
     * corrupting the saved register slots on nested calls).
     * Fall back to `sub sp,i24` (24-bit immediate) for big frames. */
    frame_sz = layout_frame_size;
    if (frame_sz > 0) {
        emit_str(EMIT_INDENT);
        if (frame_sz <= 127) {
            emit_str("add     sp,-");
            emit_int(frame_sz);
        } else {
            emit_str("sub     sp,");
            emit_int(frame_sz);
        }
        emit_nl();
    }

    /* Emit body statements */
    cg_block(body);

    /* Standard epilogue: mov sp,fp, pop r1, pop r2, pop fp, jmp (r1) */
    emit_epilogue();

    /* Emit .data for any STATIC locals (e.g. CHAR arrays with INIT) */
    cg_emit_proc_static_data(body);
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
            /* In LIBRARY mode, skip the MAIN wrapper procedure */
            if (def_defined("LIBRARY") && nd_name[child] &&
                str_eq(nd_name[child], "MAIN")) {
                child = nd_next[child];
                continue;
            }
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
