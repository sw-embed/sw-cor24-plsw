#ifndef MACRO_H
#define MACRO_H

#include "token.h"

#define MCLAUSE_EXPR    1
#define MCLAUSE_LVALUE  2
#define MCLAUSE_LABEL   3
#define MCLAUSE_REQUIRED 1
#define MCLAUSE_OPTIONAL 2
#define MACRO_MAX       8
#define MACRO_CLAUSE_MAX 8
#define MACRO_NAME_MAX  32
#define MACRO_BODY_MAX  256
#define MACRO_GEN_MAX   4
#define MACRO_GEN_LINE  64
#define MACRO_GEN_LINES 8

char  mac_name_buf[256];
int   mac_count;
char  mac_cl_name_buf[2048];
int   mac_cl_types[64];
int   mac_cl_req[64];
int   mac_cl_count[8];
char  mac_gen_buf[16384];
int   mac_gen_lcount[32];
int   mac_gen_count[8];
char  mac_body_buf[2048];

char *mac_name(int mi) {
    return mac_name_buf + mi * MACRO_NAME_MAX;
}

char *mac_body(int mi) {
    return mac_body_buf + mi * MACRO_BODY_MAX;
}

int mac_cl_idx(int mi, int ci) {
    return mi * MACRO_CLAUSE_MAX + ci;
}

char *mac_cl_name(int mi, int ci) {
    return mac_cl_name_buf + mac_cl_idx(mi, ci) * MACRO_NAME_MAX;
}

int mac_gen_lc_idx(int mi, int gi) {
    return mi * MACRO_GEN_MAX + gi;
}

char *mac_gen_line(int mi, int gi, int li) {
    int off = ((mi * MACRO_GEN_MAX + gi) * MACRO_GEN_LINES + li) * MACRO_GEN_LINE;
    return mac_gen_buf + off;
}

void mac_init(void) {
    mac_count = 0;
    int i = 0;
    while (i < MACRO_MAX) {
        mac_name(i)[0] = 0;
        mac_cl_count[i] = 0;
        mac_gen_count[i] = 0;
        mac_body(i)[0] = 0;
        i = i + 1;
    }
}

int mac_lookup(char *name) {
    int i = 0;
    while (i < mac_count) {
        if (str_eq_nocase(name, mac_name(i))) {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

/* Reopen guard - append to existing macro.h content */

int mac_parse_err;
char mac_parse_errmsg[128];

void mac_error(char *msg) {
    mac_parse_err = 1;
    str_ncopy(mac_parse_errmsg, msg, 128);
}

int mac_expect(int ttype) {
    if (cur_type != ttype) {
        mac_error("unexpected token in MACRODEF");
        return 0;
    }
    lex_scan();
    return 1;
}

int mac_parse_clause_type(void) {
    if (cur_type == TOK_IDENT) {
        if (str_eq_nocase(cur_text, "expr")) {
            lex_scan();
            return MCLAUSE_EXPR;
        }
        if (str_eq_nocase(cur_text, "lvalue")) {
            lex_scan();
            return MCLAUSE_LVALUE;
        }
        if (str_eq_nocase(cur_text, "label")) {
            lex_scan();
            return MCLAUSE_LABEL;
        }
    }
    if (cur_type == TOK_IDENT) lex_scan();
    return MCLAUSE_EXPR;
}


int mac_parse_clause(int mi) {
    int ci = mac_cl_count[mi];
    if (ci >= MACRO_CLAUSE_MAX) {
        mac_error("too many clauses in MACRODEF");
        return 0;
    }

    int idx = mac_cl_idx(mi, ci);

    if (cur_type == TOK_IDENT && str_eq_nocase(cur_text, "REQUIRED")) {
        mac_cl_req[idx] = MCLAUSE_REQUIRED;
        lex_scan();
    } else if (cur_type == TOK_IDENT && str_eq_nocase(cur_text, "OPTIONAL")) {
        mac_cl_req[idx] = MCLAUSE_OPTIONAL;
        lex_scan();
    } else {
        return 0;
    }

    if (cur_type != TOK_IDENT) {
        mac_error("expected clause name");
        return 0;
    }
    str_ncopy(mac_cl_name(mi, ci), cur_text, MACRO_NAME_MAX);
    lex_scan();

    if (!mac_expect(TOK_LPAREN)) return 0;
    mac_cl_types[idx] = mac_parse_clause_type();
    if (!mac_expect(TOK_RPAREN)) return 0;
    if (!mac_expect(TOK_SEMI)) return 0;

    mac_cl_count[mi] = ci + 1;
    return 1;
}


int mac_parse_gen(int mi) {
    int gi = mac_gen_count[mi];
    if (gi >= MACRO_GEN_MAX) {
        mac_error("too many GEN blocks");
        return 0;
    }

    lex_scan();

    if (cur_type != TOK_DO) {
        mac_error("expected DO after GEN");
        return 0;
    }
    lex_scan();

    if (!mac_expect(TOK_SEMI)) return 0;

    int lcount = 0;
    int lc_idx = mac_gen_lc_idx(mi, gi);

    while (cur_type != TOK_END && cur_type != TOK_EOF && !mac_parse_err) {
        if (cur_type == TOK_STRING) {
            if (lcount < MACRO_GEN_LINES) {
                str_ncopy(mac_gen_line(mi, gi, lcount), cur_text, MACRO_GEN_LINE);
                lcount = lcount + 1;
            }
            lex_scan();
        } else {
            lex_scan();
        }
        while (cur_type == TOK_SEMI) {
            lex_scan();
        }
    }

    mac_gen_lcount[lc_idx] = lcount;

    if (cur_type == TOK_END) {
        lex_scan();
    }
    if (cur_type == TOK_SEMI) {
        lex_scan();
    }

    mac_gen_count[mi] = gi + 1;
    return 1;
}


void mac_parse_body(int mi) {
    char *body = mac_body(mi);
    int pos = str_len(body);
    int depth = 0;

    while (cur_type != TOK_EOF && !mac_parse_err) {
        if (cur_type == TOK_END && depth == 0) {
            break;
        }

        if (cur_type == TOK_DO) {
            depth = depth + 1;
        }
        if (cur_type == TOK_END) {
            depth = depth - 1;
        }

        if (pos > 0 && pos < MACRO_BODY_MAX - 2) {
            body[pos] = 32;
            pos = pos + 1;
        }

        int tlen = str_len(cur_text);
        if (pos + tlen < MACRO_BODY_MAX - 1) {
            str_copy(body + pos, cur_text);
            pos = pos + tlen;
        }

        lex_scan();
    }

    body[pos] = 0;
}


int mac_parse_def(void) {
    mac_parse_err = 0;
    mac_parse_errmsg[0] = 0;

    if (mac_count >= MACRO_MAX) {
        mac_error("too many macro definitions");
        return -1;
    }

    int mi = mac_count;

    lex_scan();

    if (cur_type != TOK_IDENT) {
        mac_error("expected macro name after MACRODEF");
        return -1;
    }
    str_ncopy(mac_name(mi), cur_text, MACRO_NAME_MAX);
    lex_scan();

    if (!mac_expect(TOK_SEMI)) return -1;

    mac_cl_count[mi] = 0;
    mac_gen_count[mi] = 0;
    mac_body(mi)[0] = 0;

    while (!mac_parse_err && cur_type != TOK_EOF) {
        if (cur_type == TOK_IDENT &&
            (str_eq_nocase(cur_text, "REQUIRED") ||
             str_eq_nocase(cur_text, "OPTIONAL"))) {
            if (!mac_parse_clause(mi)) break;
            continue;
        }
        break;
    }

    if (mac_parse_err) return -1;

    while (cur_type != TOK_END && cur_type != TOK_EOF && !mac_parse_err) {
        if (cur_type == TOK_IDENT && str_eq_nocase(cur_text, "GEN")) {
            mac_parse_gen(mi);
            continue;
        }
        mac_parse_body(mi);
    }

    if (mac_parse_err) return -1;

    if (cur_type == TOK_END) {
        lex_scan();
    } else {
        mac_error("expected END for MACRODEF");
        return -1;
    }
    if (cur_type == TOK_SEMI) {
        lex_scan();
    }

    mac_count = mi + 1;
    return mi;
}


int mac_parse_file(void) {
    int count = 0;
    while (cur_type != TOK_EOF && !mac_parse_err) {
        if (cur_type == TOK_IDENT && str_eq_nocase(cur_text, "MACRODEF")) {
            int mi = mac_parse_def();
            if (mi >= 0) {
                count = count + 1;
            } else {
                break;
            }
        } else {
            lex_scan();
        }
    }
    return count;
}


void mac_dump(void) {
    int i = 0;
    uart_putstr("macro table: ");
    print_int(mac_count);
    uart_puts(" macros");

    while (i < mac_count) {
        uart_putstr("  MACRODEF ");
        uart_puts(mac_name(i));

        int ci = 0;
        while (ci < mac_cl_count[i]) {
            int idx = mac_cl_idx(i, ci);
            uart_putstr("    ");
            if (mac_cl_req[idx] == MCLAUSE_REQUIRED) {
                uart_putstr("REQUIRED ");
            } else {
                uart_putstr("OPTIONAL ");
            }
            uart_putstr(mac_cl_name(i, ci));
            uart_putstr("(");
            if (mac_cl_types[idx] == MCLAUSE_EXPR) {
                uart_putstr("expr");
            } else if (mac_cl_types[idx] == MCLAUSE_LVALUE) {
                uart_putstr("lvalue");
            } else if (mac_cl_types[idx] == MCLAUSE_LABEL) {
                uart_putstr("label");
            }
            uart_puts(")");
            ci = ci + 1;
        }

        int gi = 0;
        while (gi < mac_gen_count[i]) {
            int lc_idx = mac_gen_lc_idx(i, gi);
            uart_putstr("    GEN DO (");
            print_int(mac_gen_lcount[lc_idx]);
            uart_puts(" lines)");

            int li = 0;
            while (li < mac_gen_lcount[lc_idx]) {
                uart_putstr("      '");
                uart_putstr(mac_gen_line(i, gi, li));
                uart_puts("'");
                li = li + 1;
            }
            gi = gi + 1;
        }

        if (mac_body(i)[0]) {
            uart_putstr("    BODY: ");
            uart_puts(mac_body(i));
        }

        i = i + 1;
    }
}

#endif
