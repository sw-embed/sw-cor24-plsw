/* lexer.h -- PL/SW lexer for COR24 compiler */
/* Scans input buffer producing tokens */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"

/* Lexer state */
char *lex_src;    /* source buffer pointer */
int   lex_pos;    /* current position in source */
int   lex_len;    /* length of source */

/* ---- %INCLUDE processing ---- */

#define INC_MAX_DEPTH  4    /* max nesting depth */
#define INC_MAX_FILES  16   /* max registered include files */

/* Registered include files (name -> content) */
char *inc_names[INC_MAX_FILES];
char *inc_contents[INC_MAX_FILES];
int   inc_count;

/* Lexer state stack for nested includes */
char *inc_src_stack[INC_MAX_DEPTH];
int   inc_pos_stack[INC_MAX_DEPTH];
int   inc_len_stack[INC_MAX_DEPTH];
int   inc_depth;

void inc_init(void) {
    inc_count = 0;
    inc_depth = 0;
}

/* Register an include file by name and content */
void inc_register(char *name, char *content) {
    if (inc_count < INC_MAX_FILES) {
        inc_names[inc_count] = name;
        inc_contents[inc_count] = content;
        inc_count = inc_count + 1;
    }
}

/* Look up include file content by name */
char *inc_lookup(char *name) {
    int i = 0;
    while (i < inc_count) {
        if (str_eq_nocase(name, inc_names[i])) {
            return inc_contents[i];
        }
        i = i + 1;
    }
    return 0;
}

/* Push current lexer state, switch to include content */
int inc_push(char *content) {
    if (inc_depth >= INC_MAX_DEPTH) {
        return 0;
    }
    inc_src_stack[inc_depth] = lex_src;
    inc_pos_stack[inc_depth] = lex_pos;
    inc_len_stack[inc_depth] = lex_len;
    inc_depth = inc_depth + 1;

    lex_src = content;
    lex_pos = 0;
    lex_len = str_len(content);
    return 1;
}

/* Pop back to parent lexer state. Returns 1 if popped, 0 if top level. */
int inc_pop(void) {
    if (inc_depth <= 0) return 0;
    inc_depth = inc_depth - 1;
    lex_src = inc_src_stack[inc_depth];
    lex_pos = inc_pos_stack[inc_depth];
    lex_len = inc_len_stack[inc_depth];
    return 1;
}

/* Current token (globals -- no struct on stack) */
int  cur_type;
int  cur_ival;
char cur_text[TOK_TEXT_MAX];

/* Initialize lexer with a source buffer */
void lex_init(char *src, int len) {
    lex_src = src;
    lex_pos = 0;
    lex_len = len;
    inc_depth = 0;
    kw_init();
}

/* Peek at current character without advancing */
int lex_peek(void) {
    if (lex_pos >= lex_len) return 0;
    return lex_src[lex_pos];
}

/* Advance and return current character */
int lex_advance(void) {
    if (lex_pos >= lex_len) return 0;
    int c = lex_src[lex_pos];
    lex_pos = lex_pos + 1;
    return c;
}

/* Peek at next character (one ahead) */
int lex_peek2(void) {
    if (lex_pos + 1 >= lex_len) return 0;
    return lex_src[lex_pos + 1];
}

/* Character classification */
int is_alpha(int c) {
    return (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95;
}

int is_digit(int c) {
    return c >= 48 && c <= 57;
}

int is_alnum(int c) {
    return is_alpha(c) || is_digit(c);
}

int is_space(int c) {
    return c == 32 || c == 9 || c == 10 || c == 13;
}

/* Skip whitespace and comments */
void lex_skip(void) {
    while (lex_pos < lex_len) {
        int c = lex_peek();

        if (is_space(c)) {
            lex_advance();
            continue;
        }

        /* block comment: / * ... * / */
        if (c == 47 && lex_peek2() == 42) {
            lex_advance();
            lex_advance();
            while (lex_pos < lex_len) {
                if (lex_peek() == 42 && lex_peek2() == 47) {
                    lex_advance();
                    lex_advance();
                    break;
                }
                lex_advance();
            }
            continue;
        }

        break;
    }
}

/* Set current token to a simple operator */
void lex_set(int type, char *txt) {
    cur_type = type;
    cur_ival = 0;
    str_copy(cur_text, txt);
}

/* Try to process a %INCLUDE directive at the current position.
   Returns 1 if handled, 0 if not a %INCLUDE. */
int lex_try_include(void) {
    /* Save position in case this isn't %INCLUDE */
    int saved = lex_pos;

    /* We're at '%' -- advance past it */
    lex_advance();

    /* Read directive name */
    char dname[16];
    int di = 0;
    while (lex_pos < lex_len && is_alpha(lex_peek()) && di < 15) {
        dname[di] = lex_advance();
        di = di + 1;
    }
    dname[di] = 0;

    if (!str_eq_nocase(dname, "INCLUDE")) {
        /* Not %INCLUDE -- restore position */
        lex_pos = saved;
        return 0;
    }

    /* Skip whitespace after INCLUDE */
    while (lex_pos < lex_len && is_space(lex_peek())) {
        lex_advance();
    }

    /* Read the include name (until ; or whitespace) */
    char iname[TOK_TEXT_MAX];
    int ni = 0;
    while (lex_pos < lex_len && lex_peek() != 59 && !is_space(lex_peek())
           && ni < TOK_TEXT_MAX - 1) {
        iname[ni] = lex_advance();
        ni = ni + 1;
    }
    iname[ni] = 0;

    /* Skip whitespace and consume trailing semicolon */
    while (lex_pos < lex_len && is_space(lex_peek())) {
        lex_advance();
    }
    if (lex_pos < lex_len && lex_peek() == 59) {
        lex_advance();
    }

    /* Look up the include file */
    char *content = inc_lookup(iname);
    if (!content) {
        /* Try with .msw extension */
        char fname[TOK_TEXT_MAX];
        str_copy(fname, iname);
        int flen = str_len(fname);
        if (flen + 4 < TOK_TEXT_MAX) {
            fname[flen] = 46;     /* . */
            fname[flen+1] = 109;  /* m */
            fname[flen+2] = 115;  /* s */
            fname[flen+3] = 119;  /* w */
            fname[flen+4] = 0;
            content = inc_lookup(fname);
        }
    }

    if (!content) {
        /* Include not found -- not an error for now, just skip */
        return 1;
    }

    /* Push current state and switch to include content */
    if (!inc_push(content)) {
        /* Too deeply nested */
        return 1;
    }

    return 1;
}

/* Scan the next token. Result stored in cur_type/cur_ival/cur_text. */
int lex_scan(void) {
    lex_skip();

    cur_ival = 0;
    cur_text[0] = 0;

    /* Handle EOF: pop include stack if inside an include */
    if (lex_pos >= lex_len) {
        if (inc_pop()) {
            /* Resumed parent -- continue scanning */
            return lex_scan();
        }
        cur_type = TOK_EOF;
        str_copy(cur_text, "EOF");
        return TOK_EOF;
    }

    int c = lex_peek();

    /* Check for %INCLUDE directive */
    if (c == 37) { /* % */
        if (lex_try_include()) {
            /* Directive consumed -- scan next real token */
            return lex_scan();
        }
    }

    /* Identifier or keyword */
    if (is_alpha(c)) {
        int i = 0;
        while (lex_pos < lex_len && is_alnum(lex_peek()) && i < TOK_TEXT_MAX - 1) {
            cur_text[i] = lex_advance();
            i = i + 1;
        }
        cur_text[i] = 0;
        cur_type = kw_lookup(cur_text);
        return cur_type;
    }

    /* Number */
    if (is_digit(c)) {
        int i = 0;
        int val = 0;
        while (lex_pos < lex_len && is_digit(lex_peek()) && i < TOK_TEXT_MAX - 1) {
            int d = lex_advance();
            cur_text[i] = d;
            val = val * 10 + (d - 48);
            i = i + 1;
        }
        cur_text[i] = 0;
        cur_type = TOK_NUM;
        cur_ival = val;
        return TOK_NUM;
    }

    /* String literal (single-quoted) */
    if (c == 39) {
        lex_advance(); /* skip opening quote */
        int i = 0;
        while (lex_pos < lex_len && lex_peek() != 39 && i < TOK_TEXT_MAX - 1) {
            cur_text[i] = lex_advance();
            i = i + 1;
        }
        cur_text[i] = 0;
        if (lex_pos < lex_len && lex_peek() == 39) {
            lex_advance(); /* skip closing quote */
        }
        cur_type = TOK_STRING;
        return TOK_STRING;
    }

    /* Operators and punctuation */
    lex_advance(); /* consume the character */

    if (c == 43) { lex_set(TOK_PLUS, "+"); return TOK_PLUS; }
    if (c == 42) { lex_set(TOK_STAR, "*"); return TOK_STAR; }
    if (c == 47) { lex_set(TOK_SLASH, "/"); return TOK_SLASH; }
    if (c == 38) { lex_set(TOK_AMP, "&"); return TOK_AMP; }
    if (c == 124) { lex_set(TOK_PIPE, "|"); return TOK_PIPE; }
    if (c == 94) { lex_set(TOK_CARET, "^"); return TOK_CARET; }
    if (c == 126) { lex_set(TOK_TILDE, "~"); return TOK_TILDE; }
    if (c == 61) { lex_set(TOK_EQ, "="); return TOK_EQ; }
    if (c == 40) { lex_set(TOK_LPAREN, "("); return TOK_LPAREN; }
    if (c == 41) { lex_set(TOK_RPAREN, ")"); return TOK_RPAREN; }
    if (c == 44) { lex_set(TOK_COMMA, ","); return TOK_COMMA; }
    if (c == 59) { lex_set(TOK_SEMI, ";"); return TOK_SEMI; }
    if (c == 46) { lex_set(TOK_DOT, "."); return TOK_DOT; }
    if (c == 63) { lex_set(TOK_QUESTION, "?"); return TOK_QUESTION; }
    if (c == 37) { lex_set(TOK_PERCENT, "%"); return TOK_PERCENT; }
    if (c == 58) { lex_set(TOK_COLON, ":"); return TOK_COLON; }

    if (c == 45) { /* - */
        if (lex_peek() == 62) {
            lex_advance();
            lex_set(TOK_ARROW, "->");
            return TOK_ARROW;
        }
        lex_set(TOK_MINUS, "-");
        return TOK_MINUS;
    }
    if (c == 33) { /* ! */
        if (lex_peek() == 61) {
            lex_advance();
            lex_set(TOK_NE, "!=");
            return TOK_NE;
        }
        lex_set(TOK_TILDE, "!");
        return TOK_TILDE;
    }
    if (c == 60) { /* < */
        if (lex_peek() == 61) {
            lex_advance();
            lex_set(TOK_LE, "<=");
            return TOK_LE;
        }
        if (lex_peek() == 60) {
            lex_advance();
            lex_set(TOK_SHL, "<<");
            return TOK_SHL;
        }
        lex_set(TOK_LT, "<");
        return TOK_LT;
    }
    if (c == 62) { /* > */
        if (lex_peek() == 61) {
            lex_advance();
            lex_set(TOK_GE, ">=");
            return TOK_GE;
        }
        if (lex_peek() == 62) {
            lex_advance();
            lex_set(TOK_SHR, ">>");
            return TOK_SHR;
        }
        lex_set(TOK_GT, ">");
        return TOK_GT;
    }

    /* Unknown character */
    cur_text[0] = c;
    cur_text[1] = 0;
    cur_type = TOK_EOF;
    return TOK_EOF;
}

/* Print the current token for debugging */
void tok_print(void) {
    uart_putstr("  ");
    uart_putstr(tok_name(cur_type));
    if (cur_type == TOK_IDENT || cur_type == TOK_STRING) {
        uart_putstr("(");
        uart_putstr(cur_text);
        uart_putstr(")");
    } else if (cur_type == TOK_NUM) {
        uart_putstr("(");
        print_int(cur_ival);
        uart_putstr(")");
    }
    uart_putchar(10);
}

/* Tokenize a source string and print all tokens */
void lex_dump(char *label, char *src) {
    uart_putstr("--- ");
    uart_putstr(label);
    uart_puts(" ---");

    lex_init(src, str_len(src));
    while (1) {
        lex_scan();
        tok_print();
        if (cur_type == TOK_EOF) break;
    }
}

#endif
