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

/* ---- %DEFINE / %IF / %ELSE / %ENDIF conditional compilation ---- */

#define DEF_MAX  32   /* max compile-time defines */
#define COND_MAX  8   /* max %IF nesting depth */

/* Define table: name -> value (value may be empty string) */
char *def_names[DEF_MAX];
char *def_values[DEF_MAX];
int   def_count;

/* Conditional compilation stack */
int cond_active[COND_MAX];   /* 1 = emitting tokens, 0 = skipping */
int cond_seen_true[COND_MAX]; /* 1 = some branch was true (for else) */
int cond_depth;              /* current nesting depth */

void def_init(void) {
    def_count = 0;
    cond_depth = 0;
}

/* Add or update a define */
void def_set(char *name, char *value) {
    int i = 0;
    while (i < def_count) {
        if (str_eq_nocase(name, def_names[i])) {
            def_values[i] = value;
            return;
        }
        i = i + 1;
    }
    if (def_count < DEF_MAX) {
        def_names[def_count] = name;
        def_values[def_count] = value;
        def_count = def_count + 1;
    }
}

/* Check if a name is defined. Returns 1 if defined. */
int def_defined(char *name) {
    int i = 0;
    while (i < def_count) {
        if (str_eq_nocase(name, def_names[i])) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

/* Get value of a define. Returns 0 if not defined. */
char *def_get(char *name) {
    int i = 0;
    while (i < def_count) {
        if (str_eq_nocase(name, def_names[i])) {
            return def_values[i];
        }
        i = i + 1;
    }
    return 0;
}

/* Are we currently emitting tokens? (all enclosing %IFs are true) */
int cond_emitting(void) {
    if (cond_depth == 0) return 1;
    return cond_active[cond_depth - 1];
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
    def_init();
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

/* Read a directive name after '%'. Fills dname, returns length. */
int lex_read_directive(char *dname, int maxlen) {
    int di = 0;
    while (lex_pos < lex_len && is_alpha(lex_peek()) && di < maxlen - 1) {
        dname[di] = lex_advance();
        di = di + 1;
    }
    dname[di] = 0;
    return di;
}

/* Read a word (identifier) after whitespace. Returns length. */
int lex_read_word(char *buf, int maxlen) {
    while (lex_pos < lex_len && is_space(lex_peek())) {
        lex_advance();
    }
    int i = 0;
    while (lex_pos < lex_len && is_alnum(lex_peek()) && i < maxlen - 1) {
        buf[i] = lex_advance();
        i = i + 1;
    }
    buf[i] = 0;
    return i;
}

/* Skip to trailing semicolon and consume it */
void lex_skip_to_semi(void) {
    while (lex_pos < lex_len && is_space(lex_peek())) {
        lex_advance();
    }
    if (lex_pos < lex_len && lex_peek() == 59) {
        lex_advance();
    }
}

/* Process %INCLUDE directive (after directive name parsed) */
int lex_do_include(void) {
    char iname[TOK_TEXT_MAX];
    int ni = 0;

    /* Skip whitespace */
    while (lex_pos < lex_len && is_space(lex_peek())) {
        lex_advance();
    }

    /* Read the include name (until ; or whitespace) */
    while (lex_pos < lex_len && lex_peek() != 59 && !is_space(lex_peek())
           && ni < TOK_TEXT_MAX - 1) {
        iname[ni] = lex_advance();
        ni = ni + 1;
    }
    iname[ni] = 0;

    lex_skip_to_semi();

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

    if (!content) return 1;

    if (!inc_push(content)) return 1;

    return 1;
}

/* Process %DEFINE directive: %DEFINE NAME; or %DEFINE NAME value; */
int lex_do_define(void) {
    char name[TOK_TEXT_MAX];
    lex_read_word(name, TOK_TEXT_MAX);

    /* Skip whitespace -- check for value or semicolon */
    while (lex_pos < lex_len && is_space(lex_peek())) {
        lex_advance();
    }

    /* Static storage for define name and value */
    char *stored_name = arena_alloc(str_len(name) + 1);
    str_copy(stored_name, name);

    if (lex_pos < lex_len && lex_peek() != 59) {
        /* Read value (until semicolon) */
        char val[TOK_TEXT_MAX];
        int vi = 0;
        while (lex_pos < lex_len && lex_peek() != 59 && vi < TOK_TEXT_MAX - 1) {
            val[vi] = lex_advance();
            vi = vi + 1;
        }
        val[vi] = 0;
        /* Trim trailing whitespace */
        while (vi > 0 && is_space(val[vi - 1])) {
            vi = vi - 1;
            val[vi] = 0;
        }
        char *stored_val = arena_alloc(str_len(val) + 1);
        str_copy(stored_val, val);
        def_set(stored_name, stored_val);
    } else {
        char *stored_val = arena_alloc(1);
        stored_val[0] = 0;
        def_set(stored_name, stored_val);
    }

    lex_skip_to_semi();
    return 1;
}

/* Evaluate a %IF condition. Supports:
   - DEFINED(NAME) -- true if NAME is defined
   - NAME = VALUE  -- true if NAME's value equals VALUE
   - NAME          -- true if NAME is defined (shorthand) */
int lex_eval_condition(void) {
    char word[TOK_TEXT_MAX];
    lex_read_word(word, TOK_TEXT_MAX);

    if (str_eq_nocase(word, "DEFINED")) {
        /* DEFINED(NAME) */
        while (lex_pos < lex_len && is_space(lex_peek())) lex_advance();
        if (lex_pos < lex_len && lex_peek() == 40) lex_advance(); /* ( */
        char name[TOK_TEXT_MAX];
        lex_read_word(name, TOK_TEXT_MAX);
        while (lex_pos < lex_len && is_space(lex_peek())) lex_advance();
        if (lex_pos < lex_len && lex_peek() == 41) lex_advance(); /* ) */
        return def_defined(name);
    }

    /* Check for NAME = VALUE */
    while (lex_pos < lex_len && is_space(lex_peek())) lex_advance();
    if (lex_pos < lex_len && lex_peek() == 61) { /* = */
        lex_advance();
        char val[TOK_TEXT_MAX];
        lex_read_word(val, TOK_TEXT_MAX);
        char *dv = def_get(word);
        if (!dv) return 0;
        return str_eq_nocase(dv, val);
    }

    /* Bare name -- true if defined */
    return def_defined(word);
}

/* Try to process a % directive at the current position.
   Returns 1 if handled, 0 if not a known directive. */
int lex_try_directive(void) {
    int saved = lex_pos;

    /* We're at '%' -- advance past it */
    lex_advance();

    /* Read directive name */
    char dname[16];
    lex_read_directive(dname, 16);

    if (str_eq_nocase(dname, "INCLUDE")) {
        if (!cond_emitting()) {
            lex_skip_to_semi();
            return 1;
        }
        return lex_do_include();
    }

    if (str_eq_nocase(dname, "DEFINE")) {
        if (!cond_emitting()) {
            lex_skip_to_semi();
            return 1;
        }
        return lex_do_define();
    }

    if (str_eq_nocase(dname, "IF")) {
        int result = lex_eval_condition();
        lex_skip_to_semi();
        if (cond_depth < COND_MAX) {
            /* If parent is skipping, child always skips */
            int parent_active = cond_emitting();
            cond_active[cond_depth] = parent_active && result;
            cond_seen_true[cond_depth] = result;
            cond_depth = cond_depth + 1;
        }
        return 1;
    }

    if (str_eq_nocase(dname, "ELSE")) {
        lex_skip_to_semi();
        if (cond_depth > 0) {
            int idx = cond_depth - 1;
            /* Check if parent is active */
            int parent_active = 1;
            if (idx > 0) parent_active = cond_active[idx - 1];
            if (parent_active && !cond_seen_true[idx]) {
                cond_active[idx] = 1;
                cond_seen_true[idx] = 1;
            } else {
                cond_active[idx] = 0;
            }
        }
        return 1;
    }

    if (str_eq_nocase(dname, "ENDIF")) {
        lex_skip_to_semi();
        if (cond_depth > 0) {
            cond_depth = cond_depth - 1;
        }
        return 1;
    }

    /* Not a recognized directive -- restore position */
    lex_pos = saved;
    return 0;
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

    /* Check for % directives (%INCLUDE, %DEFINE, %IF, %ELSE, %ENDIF) */
    if (c == 37) { /* % */
        if (lex_try_directive()) {
            /* Directive consumed -- scan next real token */
            return lex_scan();
        }
    }

    /* If inside a false %IF branch, skip tokens until next % directive */
    if (!cond_emitting()) {
        /* Skip the current token intelligently */
        if (c == 39) { /* string literal -- skip to closing quote */
            lex_advance();
            while (lex_pos < lex_len && lex_peek() != 39) lex_advance();
            if (lex_pos < lex_len) lex_advance();
        } else if (is_alpha(c)) {
            while (lex_pos < lex_len && is_alnum(lex_peek())) lex_advance();
        } else if (is_digit(c)) {
            while (lex_pos < lex_len && is_digit(lex_peek())) lex_advance();
        } else {
            lex_advance();
        }
        return lex_scan();
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
