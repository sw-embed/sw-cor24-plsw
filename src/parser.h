/* parser.h -- PL/SW parser */
/* Parses DCL statements and expressions into AST nodes */

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

/* --- Expression parsing --- */

/* Operator precedence levels (low to high) */
#define PREC_NONE    0
#define PREC_OR      1   /* OR (logical) */
#define PREC_AND     2   /* AND (logical) */
#define PREC_BOR     3   /* | (bitwise OR) */
#define PREC_BXOR    4   /* ^ (bitwise XOR) */
#define PREC_BAND    5   /* & (bitwise AND) */
#define PREC_EQ      6   /* = != */
#define PREC_CMP     7   /* < > <= >= */
#define PREC_SHIFT   8   /* << >> */
#define PREC_ADD     9   /* + - */
#define PREC_MUL    10   /* * / */

/* Get precedence of a binary operator token */
int tok_prec(int type) {
    if (type == TOK_OR) return PREC_OR;
    if (type == TOK_AND) return PREC_AND;
    if (type == TOK_PIPE) return PREC_BOR;
    if (type == TOK_CARET) return PREC_BXOR;
    if (type == TOK_AMP) return PREC_BAND;
    if (type == TOK_EQ) return PREC_EQ;
    if (type == TOK_NE) return PREC_EQ;
    if (type == TOK_LT) return PREC_CMP;
    if (type == TOK_GT) return PREC_CMP;
    if (type == TOK_LE) return PREC_CMP;
    if (type == TOK_GE) return PREC_CMP;
    if (type == TOK_SHL) return PREC_SHIFT;
    if (type == TOK_SHR) return PREC_SHIFT;
    if (type == TOK_PLUS) return PREC_ADD;
    if (type == TOK_MINUS) return PREC_ADD;
    if (type == TOK_STAR) return PREC_MUL;
    if (type == TOK_SLASH) return PREC_MUL;
    return PREC_NONE;
}

/* Forward declarations */
int parse_expr(void);
int parse_expr_prec(int min_prec);

/* Parse a primary expression (atom + postfix) */
int parse_primary(void) {
    /* Unary minus */
    if (cur_type == TOK_MINUS) {
        parse_advance();
        int operand = parse_primary();
        return nd_unop(TOK_MINUS, operand);
    }
    /* Bitwise NOT ~ */
    if (cur_type == TOK_TILDE) {
        parse_advance();
        int operand = parse_primary();
        return nd_unop(TOK_TILDE, operand);
    }
    /* Logical NOT */
    if (cur_type == TOK_NOT) {
        parse_advance();
        int operand = parse_primary();
        return nd_unop(TOK_NOT, operand);
    }

    /* ADDR(expr) -- address-of */
    if (cur_type == TOK_ADDR) {
        parse_advance();
        parse_expect(TOK_LPAREN);
        int operand = parse_expr();
        parse_expect(TOK_RPAREN);
        int n = nd_alloc(NODE_ADDR);
        if (n != NODE_NULL) {
            nd_left[n] = operand;
        }
        return n;
    }

    /* Parenthesized sub-expression */
    if (cur_type == TOK_LPAREN) {
        parse_advance();
        int e = parse_expr();
        parse_expect(TOK_RPAREN);
        return e;
    }

    /* Number literal */
    if (cur_type == TOK_NUM) {
        int n = nd_literal(cur_ival);
        parse_advance();
        return n;
    }

    /* String literal */
    if (cur_type == TOK_STRING) {
        int n = nd_alloc(NODE_LITERAL);
        if (n != NODE_NULL) {
            nd_set_name(n, cur_text);
            nd_type[n] = TYPE_CHAR;
        }
        parse_advance();
        return n;
    }

    /* Identifier -- may be followed by call(args) */
    if (cur_type == TOK_IDENT) {
        int nlen = str_len(cur_text);
        char *name = arena_alloc(nlen + 1);
        if (name) str_copy(name, cur_text);
        parse_advance();

        /* Function call / array subscript: name(args...) */
        if (cur_type == TOK_LPAREN) {
            parse_advance();
            int args = NODE_NULL;
            int tail = NODE_NULL;
            if (cur_type != TOK_RPAREN) {
                args = parse_expr();
                tail = args;
                while (cur_type == TOK_COMMA) {
                    parse_advance();
                    int a = parse_expr();
                    if (tail != NODE_NULL) {
                        nd_next[tail] = a;
                    }
                    tail = a;
                }
            }
            parse_expect(TOK_RPAREN);
            int n = nd_alloc(NODE_CALL);
            if (n != NODE_NULL) {
                nd_name[n] = name;
                nd_left[n] = args;
            }
            return n;
        }

        /* Plain identifier */
        int n = nd_alloc(NODE_IDENT);
        if (n != NODE_NULL) {
            nd_name[n] = name;
        }
        return n;
    }

    parse_error("expected expression");
    return NODE_NULL;
}

/* Parse postfix operators: .field and ->field */
int parse_postfix(int left) {
    while (!parse_err) {
        if (cur_type == TOK_DOT) {
            parse_advance();
            if (cur_type != TOK_IDENT) {
                parse_error("expected field name");
                return left;
            }
            int n = nd_alloc(NODE_FIELD_ACCESS);
            if (n != NODE_NULL) {
                nd_left[n] = left;
                nd_set_name(n, cur_text);
            }
            parse_advance();
            left = n;
            continue;
        }
        if (cur_type == TOK_ARROW) {
            parse_advance();
            if (cur_type != TOK_IDENT) {
                parse_error("expected field name after ->");
                return left;
            }
            int n = nd_alloc(NODE_DEREF);
            if (n != NODE_NULL) {
                nd_left[n] = left;
                nd_set_name(n, cur_text);
            }
            parse_advance();
            left = n;
            continue;
        }
        break;
    }
    return left;
}

/* Precedence-climbing expression parser */
int parse_expr_prec(int min_prec) {
    int left = parse_primary();
    if (left == NODE_NULL) return NODE_NULL;
    left = parse_postfix(left);

    while (!parse_err) {
        int prec = tok_prec(cur_type);
        if (prec == PREC_NONE || prec < min_prec) break;

        int op = cur_type;
        parse_advance();
        int right = parse_expr_prec(prec + 1);
        left = nd_binop(op, left, right);
        left = parse_postfix(left);
    }

    return left;
}

/* Parse a full expression */
int parse_expr(void) {
    return parse_expr_prec(PREC_OR);
}

#endif
