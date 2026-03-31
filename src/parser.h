/* parser.h -- PL/SW declaration parser */
/* Parses DCL statements into AST nodes */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

/* Parser error state */
int parse_err;          /* 0 = ok, 1 = error */
char parse_errmsg[64];  /* error description */

void parse_init(char *src) {
    parse_err = 0;
    parse_errmsg[0] = 0;
    lex_init(src, str_len(src));
    lex_scan();  /* prime the first token */
}

/* Report a parse error */
void parse_error(char *msg) {
    parse_err = 1;
    str_copy(parse_errmsg, msg);
    uart_putstr("ERROR: ");
    uart_puts(msg);
}

/* Check if current token matches type */
int parse_check(int type) {
    return cur_type == type;
}

/* Consume current token and advance */
void parse_advance(void) {
    lex_scan();
}

/* Expect a specific token; consume and advance. Error if wrong. */
int parse_expect(int type) {
    if (cur_type != type) {
        uart_putstr("expected ");
        uart_putstr(tok_name(type));
        uart_putstr(" got ");
        uart_putstr(tok_name(cur_type));
        uart_putchar(10);
        parse_error("unexpected token");
        return 0;
    }
    parse_advance();
    return 1;
}

/* Try to match a token; consume if matched, return 1. Else return 0. */
int parse_match(int type) {
    if (cur_type == type) {
        parse_advance();
        return 1;
    }
    return 0;
}

/* --- Type parsing --- */

/* Parse a type keyword and return the TYPE_ constant.
   Handles INT(n) where n determines INT8/INT16/INT24.
   Returns TYPE_NONE if no type keyword found. */
int parse_type(void) {
    if (cur_type == TOK_INT) {
        parse_advance();
        /* optional width: INT(8), INT(16), INT(24) */
        if (parse_check(TOK_LPAREN)) {
            parse_advance();
            int width = cur_ival;
            parse_expect(TOK_NUM);
            parse_expect(TOK_RPAREN);
            if (width <= 8) return TYPE_INT8;
            if (width <= 16) return TYPE_INT16;
            return TYPE_INT24;
        }
        return TYPE_INT24;  /* default INT = INT(24) */
    }
    if (cur_type == TOK_BYTE) { parse_advance(); return TYPE_BYTE; }
    if (cur_type == TOK_CHAR) { parse_advance(); return TYPE_CHAR; }
    if (cur_type == TOK_PTR)  { parse_advance(); return TYPE_PTR; }
    if (cur_type == TOK_WORD) { parse_advance(); return TYPE_WORD; }
    if (cur_type == TOK_BIT)  { parse_advance(); return TYPE_BIT; }
    return TYPE_NONE;
}

/* --- Storage class parsing --- */

/* Parse optional storage class. Returns STOR_ constant. */
int parse_storage(void) {
    if (cur_type == TOK_STATIC)   { parse_advance(); return STOR_STATIC; }
    if (cur_type == TOK_AUTO)     { parse_advance(); return STOR_AUTO; }
    if (cur_type == TOK_EXTERNAL) { parse_advance(); return STOR_EXTERNAL; }
    if (cur_type == TOK_BASED)    { parse_advance(); return STOR_BASED; }
    return STOR_AUTO;
}

/* --- INIT parsing --- */

/* Parse INIT(value) -- returns node index or NODE_NULL */
int parse_init_attr(void) {
    if (cur_type != TOK_INIT) return NODE_NULL;
    parse_advance();  /* consume INIT */
    parse_expect(TOK_LPAREN);
    int init_node = NODE_NULL;
    if (cur_type == TOK_NUM) {
        init_node = nd_literal(cur_ival);
        parse_advance();
    } else if (cur_type == TOK_STRING) {
        init_node = nd_alloc(NODE_LITERAL);
        if (init_node != NODE_NULL) {
            nd_set_name(init_node, cur_text);
            nd_type[init_node] = TYPE_CHAR;
        }
        parse_advance();
    } else {
        parse_error("expected literal in INIT");
        return NODE_NULL;
    }
    parse_expect(TOK_RPAREN);
    return init_node;
}

/* --- DCL parsing --- */

/* Parse a single scalar/array declaration field.
   Used for both top-level DCL and level-based fields.
   Returns the DCL node index. */
int parse_dcl_field(int level) {
    if (cur_type != TOK_IDENT) {
        parse_error("expected identifier in DCL");
        return NODE_NULL;
    }

    /* Save name in arena (avoids large stack local) */
    int nlen = str_len(cur_text);
    char *name = arena_alloc(nlen + 1);
    if (!name) {
        parse_error("arena full");
        return NODE_NULL;
    }
    str_copy(name, cur_text);
    parse_advance();

    /* Check for array dimension: NAME(dim) */
    int dim = 0;
    if (parse_check(TOK_LPAREN)) {
        parse_advance();
        if (cur_type == TOK_NUM) {
            dim = cur_ival;
            parse_advance();
        } else {
            parse_error("expected number for array dimension");
            return NODE_NULL;
        }
        parse_expect(TOK_RPAREN);
    }

    /* Parse type */
    int type = parse_type();

    /* Parse storage class */
    int stor = parse_storage();

    /* Parse optional INIT */
    int init = parse_init_attr();

    /* Build the DCL node */
    int n = nd_alloc(NODE_DCL);
    if (n == NODE_NULL) {
        parse_error("node pool full");
        return NODE_NULL;
    }
    /* name is already in arena, just assign directly */
    nd_name[n] = name;
    nd_type[n] = type;
    nd_stor[n] = stor;
    nd_level[n] = level;
    nd_ival[n] = dim;
    nd_right[n] = init;  /* init expression, or NODE_NULL */

    return n;
}

/* Parse a level-based record declaration.
   Called after DCL and the first level number have been consumed.
   Syntax: DCL 1 NAME, 3 FIELD TYPE, 3 FIELD TYPE, ...;

   Level 1 = record container, 3+ = fields.
   Lower level numbers start new groups. */
int parse_dcl_record(int first_level) {
    /* Parse the record name */
    if (cur_type != TOK_IDENT) {
        parse_error("expected record name");
        return NODE_NULL;
    }

    /* Create record container node */
    int rec = nd_alloc(NODE_DCL);
    if (rec == NODE_NULL) {
        parse_error("node pool full");
        return NODE_NULL;
    }
    nd_set_name(rec, cur_text);
    nd_type[rec] = TYPE_RECORD;
    nd_level[rec] = first_level;
    parse_advance();

    /* Parse optional storage class on record itself */
    nd_stor[rec] = parse_storage();

    /* Expect comma or semicolon after record name */
    if (!parse_match(TOK_COMMA)) {
        /* Record with no fields -- just a named type */
        parse_expect(TOK_SEMI);
        return rec;
    }

    /* Parse fields: level NAME type, ... ; */
    while (!parse_err) {
        /* Expect a level number */
        if (cur_type != TOK_NUM) {
            parse_error("expected level number");
            return rec;
        }
        int field_level = cur_ival;
        parse_advance();

        /* Parse the field */
        int field = parse_dcl_field(field_level);
        if (field == NODE_NULL) return rec;

        /* Append field as child of record */
        nd_append(rec, field);

        /* Comma means more fields, semicolon ends */
        if (parse_check(TOK_SEMI)) {
            parse_advance();
            break;
        }
        if (!parse_match(TOK_COMMA)) {
            parse_error("expected , or ; in record DCL");
            return rec;
        }
    }

    return rec;
}

/* Parse a DCL statement. Handles:
   - DCL NAME TYPE;
   - DCL NAME(dim) TYPE;
   - DCL NAME TYPE STATIC;
   - DCL NAME TYPE INIT(val);
   - DCL 1 NAME, 3 FIELD TYPE, ...;
   Returns the root DCL node index. */
int parse_dcl(void) {
    /* Consume DCL or DECLARE */
    if (cur_type != TOK_DCL && cur_type != TOK_DECLARE) {
        parse_error("expected DCL");
        return NODE_NULL;
    }
    parse_advance();

    /* Check if level-based record: DCL <number> ... */
    if (cur_type == TOK_NUM) {
        int level = cur_ival;
        parse_advance();
        return parse_dcl_record(level);
    }

    /* Simple scalar or array declaration */
    int n = parse_dcl_field(0);
    if (n == NODE_NULL) return NODE_NULL;

    /* Expect semicolon */
    parse_expect(TOK_SEMI);

    return n;
}

#endif
