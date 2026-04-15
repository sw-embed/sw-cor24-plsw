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
#define MACRO_GEN_LINES 24

char  mac_name_buf[256];
int   mac_count;
char  mac_cl_name_buf[2048];
int   mac_cl_types[64];
int   mac_cl_req[64];
int   mac_cl_count[8];
char  mac_gen_buf[49152];
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

    /* Clause name may be an identifier or a keyword (e.g. PTR, INT)
       since the name is just a macro parameter label */
    if (cur_text[0] == 0) {
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
            if (lcount >= MACRO_GEN_LINES) {
                mac_error("too many lines in GEN DO block");
                return 0;
            }
            str_ncopy(mac_gen_line(mi, gi, lcount), cur_text, MACRO_GEN_LINE);
            lcount = lcount + 1;
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

/* ---- Macro Expansion ---- */

#define MAC_ARG_MAX     8
#define MAC_ARG_VAL_MAX 64
#define MAC_EXPAND_MAX  512

/* Argument values during expansion (parallel to clauses) */
char  mac_arg_buf[MAC_ARG_MAX * MAC_ARG_VAL_MAX];
int   mac_arg_set[MAC_ARG_MAX];

/* Expansion output buffer */
char  mac_expand_buf[MAC_EXPAND_MAX];
int   mac_expand_len;
int   mac_expand_err;
char  mac_expand_errmsg[128];

char *mac_arg_val(int ai) {
    return mac_arg_buf + ai * MAC_ARG_VAL_MAX;
}

void mac_exp_error(char *msg) {
    mac_expand_err = 1;
    str_ncopy(mac_expand_errmsg, msg, 128);
}

/* Append string to expansion buffer */
void mac_exp_append(char *s) {
    int slen = str_len(s);
    if (mac_expand_len + slen < MAC_EXPAND_MAX - 1) {
        str_copy(mac_expand_buf + mac_expand_len, s);
        mac_expand_len = mac_expand_len + slen;
    }
}

/* Append a single char to expansion buffer */
void mac_exp_appendc(int c) {
    if (mac_expand_len < MAC_EXPAND_MAX - 2) {
        mac_expand_buf[mac_expand_len] = c;
        mac_expand_len = mac_expand_len + 1;
        mac_expand_buf[mac_expand_len] = 0;
    }
}

/* Resolve ADDR(var) to assembly label _var for GEN substitution.
 * Argument text arrives as "ADDR ( VARNAME )" (tokens with spaces).
 * Returns 1 if resolved (appends _VARNAME to expansion buffer), 0 if not ADDR. */
int mac_resolve_addr(char *val) {
    int i = 0;
    int ni;
    char name[MACRO_NAME_MAX];

    /* Skip leading whitespace */
    while (val[i] == 32) i = i + 1;

    /* Check for ADDR (case-insensitive) */
    if ((val[i] == 'A' || val[i] == 'a') &&
        (val[i+1] == 'D' || val[i+1] == 'd') &&
        (val[i+2] == 'D' || val[i+2] == 'd') &&
        (val[i+3] == 'R' || val[i+3] == 'r') &&
        !is_alnum(val[i+4])) {
        i = i + 4;
    } else {
        return 0;
    }

    /* Skip whitespace and expect '(' */
    while (val[i] == 32) i = i + 1;
    if (val[i] != 40) return 0; /* '(' */
    i = i + 1;

    /* Skip whitespace, extract identifier */
    while (val[i] == 32) i = i + 1;
    ni = 0;
    while (is_alnum(val[i]) || val[i] == 95) { /* alnum or '_' */
        if (ni < MACRO_NAME_MAX - 1) {
            name[ni] = val[i];
            ni = ni + 1;
        }
        i = i + 1;
    }
    name[ni] = 0;
    if (ni == 0) return 0;

    /* Skip whitespace and expect ')' */
    while (val[i] == 32) i = i + 1;
    if (val[i] != 41) return 0; /* ')' */
    i = i + 1;

    /* Should be end of string (maybe trailing whitespace) */
    while (val[i] == 32) i = i + 1;
    if (val[i] != 0) return 0;

    /* Emit assembly label: _VARNAME */
    mac_exp_appendc(95); /* '_' */
    mac_exp_append(name);
    return 1;
}

/* Substitute {CLAUSE_NAME} in a GEN line template with argument values.
 * Result appended to expansion buffer as inline ASM. */
void mac_gen_substitute(int mi, char *tmpl) {
    int i = 0;
    int tlen = str_len(tmpl);

    /* Start with ASM emit: indent + instruction */
    mac_exp_append("ASM DO; '");

    while (i < tlen) {
        if (tmpl[i] == 123) { /* '{' */
            /* Extract placeholder name */
            char pname[MACRO_NAME_MAX];
            int pi = 0;
            i = i + 1;
            while (i < tlen && tmpl[i] != 125 && pi < MACRO_NAME_MAX - 1) {
                pname[pi] = tmpl[i];
                pi = pi + 1;
                i = i + 1;
            }
            pname[pi] = 0;
            if (i < tlen && tmpl[i] == 125) { /* '}' */
                i = i + 1;
            }

            /* Find matching clause */
            int ci = 0;
            int found = 0;
            while (ci < mac_cl_count[mi]) {
                if (str_eq_nocase(pname, mac_cl_name(mi, ci))) {
                    if (mac_arg_set[ci]) {
                        /* In GEN context, resolve ADDR(var) to _var */
                        if (!mac_resolve_addr(mac_arg_val(ci))) {
                            mac_exp_append(mac_arg_val(ci));
                        }
                    }
                    found = 1;
                    break;
                }
                ci = ci + 1;
            }
            if (!found) {
                /* Not a clause -- emit as-is */
                mac_exp_appendc(123);
                mac_exp_append(pname);
                mac_exp_appendc(125);
            }
        } else {
            mac_exp_appendc(tmpl[i]);
            i = i + 1;
        }
    }

    mac_exp_append("'; END;");
}

/* Substitute clause references in body text.
 * In the body, bare clause names are replaced with argument values. */
void mac_body_substitute(int mi) {
    char *body = mac_body(mi);
    int blen = str_len(body);
    int i = 0;

    while (i < blen) {
        /* Try to match an identifier */
        if (is_alpha(body[i])) {
            char word[MACRO_NAME_MAX];
            int wi = 0;
            while (i < blen && is_alnum(body[i]) && wi < MACRO_NAME_MAX - 1) {
                word[wi] = body[i];
                wi = wi + 1;
                i = i + 1;
            }
            word[wi] = 0;

            /* Check if this is a clause name */
            int ci = 0;
            int found = 0;
            while (ci < mac_cl_count[mi]) {
                if (str_eq_nocase(word, mac_cl_name(mi, ci))) {
                    if (mac_arg_set[ci]) {
                        mac_exp_append(mac_arg_val(ci));
                    } else {
                        mac_exp_appendc(48); /* '0' for unset optional */
                    }
                    found = 1;
                    break;
                }
                ci = ci + 1;
            }
            if (!found) {
                mac_exp_append(word);
            }
        } else {
            mac_exp_appendc(body[i]);
            i = i + 1;
        }
    }
}

/* Parse macro invocation arguments: ?NAME(KEYWORD(value), ...).
 * Assumes cur_type == TOK_QUESTION and the next token is the macro name.
 * Returns macro index or -1 on error. */
int mac_parse_invoke(void) {
    mac_expand_err = 0;
    mac_expand_errmsg[0] = 0;

    /* cur_type should be TOK_QUESTION already consumed;
     * next token is the macro name (IDENT) */
    lex_scan(); /* get macro name */

    if (cur_type != TOK_IDENT) {
        mac_exp_error("expected macro name after ?");
        return -1;
    }

    int mi = mac_lookup(cur_text);
    if (mi < 0) {
        mac_exp_error("unknown macro");
        return -1;
    }

    /* Clear argument values */
    int ai = 0;
    while (ai < MAC_ARG_MAX) {
        mac_arg_set[ai] = 0;
        mac_arg_val(ai)[0] = 0;
        ai = ai + 1;
    }

    lex_scan(); /* expect '(' */
    if (cur_type != TOK_LPAREN) {
        mac_exp_error("expected ( after macro name");
        return -1;
    }

    lex_scan(); /* first keyword or ')' */

    /* Parse keyword arguments: KEYWORD(value), ... */
    while (cur_type != TOK_RPAREN && cur_type != TOK_EOF && !mac_expand_err) {
        if (cur_type != TOK_IDENT) {
            mac_exp_error("expected keyword in macro invocation");
            return -1;
        }

        /* Find matching clause */
        char kw[MACRO_NAME_MAX];
        str_ncopy(kw, cur_text, MACRO_NAME_MAX);
        int ci = 0;
        int found = -1;
        while (ci < mac_cl_count[mi]) {
            if (str_eq_nocase(kw, mac_cl_name(mi, ci))) {
                found = ci;
                break;
            }
            ci = ci + 1;
        }

        if (found < 0) {
            mac_exp_error("unknown keyword in macro invocation");
            return -1;
        }

        lex_scan(); /* expect '(' */
        if (cur_type != TOK_LPAREN) {
            mac_exp_error("expected ( after keyword");
            return -1;
        }

        /* Collect value tokens until matching ')' */
        lex_scan();
        char *val = mac_arg_val(found);
        int vpos = 0;
        int depth = 0;

        while (cur_type != TOK_EOF && !mac_expand_err) {
            if (cur_type == TOK_LPAREN) {
                depth = depth + 1;
            }
            if (cur_type == TOK_RPAREN) {
                if (depth == 0) break;
                depth = depth - 1;
            }

            /* Append token text to value */
            int tlen = str_len(cur_text);
            if (vpos > 0 && vpos < MAC_ARG_VAL_MAX - 2) {
                val[vpos] = 32; /* space between tokens */
                vpos = vpos + 1;
            }
            if (vpos + tlen < MAC_ARG_VAL_MAX - 1) {
                str_copy(val + vpos, cur_text);
                vpos = vpos + tlen;
            }
            lex_scan();
        }
        val[vpos] = 0;
        mac_arg_set[found] = 1;

        lex_scan(); /* skip ')' of the keyword arg */

        if (cur_type == TOK_COMMA) {
            lex_scan(); /* skip comma, get next keyword */
        }
    }

    if (cur_type == TOK_RPAREN) {
        lex_scan(); /* skip closing ')' of invocation */
    }

    /* Check required clauses */
    ci = 0;
    while (ci < mac_cl_count[mi]) {
        int idx = mac_cl_idx(mi, ci);
        if (mac_cl_req[idx] == MCLAUSE_REQUIRED && !mac_arg_set[ci]) {
            mac_exp_error("missing required keyword");
            return -1;
        }
        ci = ci + 1;
    }

    return mi;
}

/* Expand macro mi into mac_expand_buf. Call after mac_parse_invoke(). */
void mac_expand(int mi) {
    mac_expand_len = 0;
    mac_expand_buf[0] = 0;

    /* Emit GEN blocks first (each becomes ASM DO; ...; END;) */
    int gi = 0;
    while (gi < mac_gen_count[mi]) {
        int lc_idx = mac_gen_lc_idx(mi, gi);
        int lcount = mac_gen_lcount[lc_idx];
        int li = 0;
        while (li < lcount) {
            if (mac_expand_len > 0) {
                mac_exp_appendc(32); /* space separator */
            }
            mac_gen_substitute(mi, mac_gen_line(mi, gi, li));
            li = li + 1;
        }
        gi = gi + 1;
    }

    /* Expand body text (with clause name substitution) */
    if (mac_body(mi)[0]) {
        if (mac_expand_len > 0) {
            mac_exp_appendc(32);
        }
        mac_body_substitute(mi);
    }
}

/* Full macro invocation: parse ?NAME(...) and expand.
 * Returns pointer to expansion buffer, or 0 on error. */
char *mac_invoke(void) {
    int mi = mac_parse_invoke();
    if (mi < 0) return 0;

    mac_expand(mi);
    return mac_expand_buf;
}

#endif
