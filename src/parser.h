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

/* --- Procedure parsing --- */

/* Option flag bits for PROC OPTIONS */
#define OPT_FREESTANDING 1
#define OPT_NAKED        2
#define OPT_LEAF         4

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
int parse_stmt(void);

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

/* --- Statement parsing --- */

/* Parse a block of statements (until END or EOF).
   Returns a BLOCK node with statements as children. */
int parse_block(void) {
    int blk = nd_alloc(NODE_BLOCK);
    while (!parse_err && cur_type != TOK_END && cur_type != TOK_EOF) {
        int s = parse_stmt();
        if (s != NODE_NULL) {
            nd_append(blk, s);
        }
    }
    return blk;
}

/* Parse a single statement.
   Handles: assignment, IF, DO (WHILE/count/block), CALL,
   RETURN, DCL, and expression statements. */
int parse_stmt(void) {
    int n;
    int cond;
    int then_body;
    int else_body;
    int body;
    int args;
    int tail;
    int expr;
    int lhs;
    int rhs;
    int start_expr;
    int end_expr;
    char *name;
    int nlen;
    int stmt_line;

    if (parse_err) return NODE_NULL;

    /* Capture line number at start of statement for listing */
    stmt_line = lex_line;

    /* ?MACRO(...) invocation -- expand and re-parse */
    if (cur_type == TOK_QUESTION) {
        char *expansion = mac_invoke();
        if (expansion && expansion[0]) {
            if (inc_push(expansion)) {
                lex_scan();  /* start scanning the expansion */
                return parse_stmt();
            }
        }
        parse_error("macro invocation failed");
        return NODE_NULL;
    }

    /* IF (cond) THEN stmt ELSE stmt */
    if (cur_type == TOK_IF) {
        parse_advance();
        cond = parse_expr();
        parse_expect(TOK_THEN);

        /* Then branch: DO;...END; block or single statement */
        if (cur_type == TOK_DO) {
            parse_advance();
            parse_expect(TOK_SEMI);
            then_body = parse_block();
            parse_expect(TOK_END);
            parse_expect(TOK_SEMI);
        } else {
            then_body = parse_stmt();
        }

        /* Optional ELSE */
        else_body = NODE_NULL;
        if (cur_type == TOK_ELSE) {
            parse_advance();
            if (cur_type == TOK_DO) {
                parse_advance();
                parse_expect(TOK_SEMI);
                else_body = parse_block();
                parse_expect(TOK_END);
                parse_expect(TOK_SEMI);
            } else {
                else_body = parse_stmt();
            }
        }

        n = nd_alloc(NODE_IF);
        if (n != NODE_NULL) {
            nd_left[n] = cond;
            nd_right[n] = then_body;
            nd_ival[n] = else_body;  /* else branch index, or NODE_NULL */
        }
        return n;
    }

    /* DO variants */
    if (cur_type == TOK_DO) {
        parse_advance();

        /* DO WHILE (cond); ... END; */
        if (cur_type == TOK_WHILE) {
            parse_advance();
            parse_expect(TOK_LPAREN);
            cond = parse_expr();
            parse_expect(TOK_RPAREN);
            parse_expect(TOK_SEMI);
            body = parse_block();
            parse_expect(TOK_END);
            parse_expect(TOK_SEMI);

            n = nd_alloc(NODE_DO_WHILE);
            if (n != NODE_NULL) {
                nd_left[n] = cond;
                nd_right[n] = body;
            }
            return n;
        }

        /* DO I = start TO end; ... END; */
        if (cur_type == TOK_IDENT) {
            nlen = str_len(cur_text);
            name = arena_alloc(nlen + 1);
            if (name) str_copy(name, cur_text);
            parse_advance();

            if (cur_type == TOK_EQ) {
                parse_advance();
                start_expr = parse_expr();
                parse_expect(TOK_TO);
                end_expr = parse_expr();
                parse_expect(TOK_SEMI);
                body = parse_block();
                parse_expect(TOK_END);
                parse_expect(TOK_SEMI);

                n = nd_alloc(NODE_DO_COUNT);
                if (n != NODE_NULL) {
                    nd_name[n] = name;
                    nd_left[n] = start_expr;
                    nd_ival[n] = end_expr;  /* end expr node index */
                    nd_right[n] = body;
                }
                return n;
            }

            parse_error("expected = in DO count");
            return NODE_NULL;
        }

        /* DO; ... END; (simple block) */
        parse_expect(TOK_SEMI);
        body = parse_block();
        parse_expect(TOK_END);
        parse_expect(TOK_SEMI);
        return body;
    }

    /* CALL proc(args); -- proc name may be a keyword token */
    if (cur_type == TOK_CALL) {
        parse_advance();
        if (!is_alpha(cur_text[0])) {
            parse_error("expected proc name after CALL");
            return NODE_NULL;
        }
        nlen = str_len(cur_text);
        name = arena_alloc(nlen + 1);
        if (name) str_copy(name, cur_text);
        parse_advance();

        args = NODE_NULL;
        if (cur_type == TOK_LPAREN) {
            parse_advance();
            tail = NODE_NULL;
            if (cur_type != TOK_RPAREN) {
                args = parse_expr();
                tail = args;
                while (cur_type == TOK_COMMA) {
                    parse_advance();
                    expr = parse_expr();
                    if (tail != NODE_NULL) nd_next[tail] = expr;
                    tail = expr;
                }
            }
            parse_expect(TOK_RPAREN);
        }
        parse_expect(TOK_SEMI);

        n = nd_alloc(NODE_CALL);
        if (n != NODE_NULL) {
            nd_name[n] = name;
            nd_left[n] = args;
            nd_line[n] = stmt_line;
        }
        return n;
    }

    /* RETURN(expr); or RETURN; */
    if (cur_type == TOK_RETURN) {
        parse_advance();
        expr = NODE_NULL;
        if (cur_type == TOK_LPAREN) {
            parse_advance();
            expr = parse_expr();
            parse_expect(TOK_RPAREN);
        }
        parse_expect(TOK_SEMI);
        n = nd_return(expr);
        if (n != NODE_NULL) nd_line[n] = stmt_line;
        return n;
    }

    /* ASM DO; 'instr'; ... END; */
    if (cur_type == TOK_ASM) {
        parse_advance();
        parse_expect(TOK_DO);
        parse_expect(TOK_SEMI);

        n = nd_alloc(NODE_ASM_BLOCK);
        if (n == NODE_NULL) {
            parse_error("node pool full");
            return NODE_NULL;
        }

        /* Collect string literals as children */
        while (!parse_err && cur_type != TOK_END && cur_type != TOK_EOF) {
            if (cur_type == TOK_STRING) {
                /* Each asm line stored as LITERAL node with nd_name */
                lhs = nd_alloc(NODE_LITERAL);
                if (lhs != NODE_NULL) {
                    nlen = str_len(cur_text);
                    name = arena_alloc(nlen + 1);
                    if (name) str_copy(name, cur_text);
                    nd_name[lhs] = name;
                }
                nd_append(n, lhs);
                parse_advance();
                parse_expect(TOK_SEMI);
            } else {
                parse_error("expected string literal in ASM block");
                return NODE_NULL;
            }
        }
        parse_expect(TOK_END);
        parse_expect(TOK_SEMI);
        return n;
    }

    /* DCL / DECLARE */
    if (cur_type == TOK_DCL || cur_type == TOK_DECLARE) {
        return parse_dcl();
    }

    /* Assignment or expression statement.
       Parse lvalue (primary + postfix), then check for = */
    lhs = parse_primary();
    if (lhs == NODE_NULL) return NODE_NULL;
    lhs = parse_postfix(lhs);

    if (cur_type == TOK_EQ) {
        parse_advance();
        rhs = parse_expr();
        parse_expect(TOK_SEMI);

        /* Convert CALL to ARRAY_ACCESS for subscript assignment */
        if (nd_kind[lhs] == NODE_CALL) {
            nd_kind[lhs] = NODE_ARRAY_ACCESS;
        }

        n = nd_assign(lhs, rhs);
        if (n != NODE_NULL) nd_line[n] = stmt_line;
        return n;
    }

    /* Expression statement (e.g., bare function call) */
    parse_expect(TOK_SEMI);
    if (lhs != NODE_NULL) nd_line[lhs] = stmt_line;
    return lhs;
}

/* --- Top-level parser --- */

/* Forward declaration */
int parse_proc(void);

/* Parse a complete PL/SW program.
   Top-level constructs: DCL declarations and PROC definitions.
   Returns a PROGRAM node with all top-level items as children. */
int parse_program(void) {
    int prog = nd_alloc(NODE_PROGRAM);
    if (prog == NODE_NULL) {
        parse_error("node pool full");
        return NODE_NULL;
    }

    while (!parse_err && cur_type != TOK_EOF) {
        int child = NODE_NULL;

        if (cur_type == TOK_DCL || cur_type == TOK_DECLARE) {
            child = parse_dcl();
        } else if (cur_type == TOK_PROC) {
            child = parse_proc();
        } else {
            /* Skip label: NAME COLON before PROC, or MACRODEF */
            if (cur_type == TOK_IDENT) {
                /* Check for MACRODEF -- parse macro definition */
                if (str_eq_nocase(cur_text, "MACRODEF")) {
                    mac_parse_def();
                    continue;
                }
                /* Look ahead: could be LABEL: PROC */
                int nlen = str_len(cur_text);
                char *name = arena_alloc(nlen + 1);
                if (name) str_copy(name, cur_text);
                parse_advance();
                if (cur_type == TOK_COLON) {
                    parse_advance();
                    /* Now expect PROC */
                    if (cur_type == TOK_PROC) {
                        child = parse_proc();
                        /* Overwrite the PROC name with the label */
                        if (child != NODE_NULL) {
                            nd_name[child] = name;
                        }
                    } else {
                        parse_error("expected PROC after label");
                        break;
                    }
                } else {
                    parse_error("unexpected top-level token");
                    break;
                }
            } else {
                parse_error("unexpected top-level token");
                break;
            }
        }

        if (child != NODE_NULL) {
            nd_append(prog, child);
        }
    }

    return prog;
}

/* --- Procedure parser --- */

/* Parse a parameter: NAME TYPE
   Returns a PARAM node with name and type set. */
int parse_param(void) {
    int n;
    int nlen;
    char *name;
    int type;

    if (!is_alpha(cur_text[0])) {
        parse_error("expected param name");
        return NODE_NULL;
    }
    nlen = str_len(cur_text);
    name = arena_alloc(nlen + 1);
    if (name) str_copy(name, cur_text);
    parse_advance();

    type = parse_type();

    n = nd_alloc(NODE_PARAM);
    if (n != NODE_NULL) {
        nd_name[n] = name;
        nd_type[n] = type;
    }
    return n;
}

/* Parse PROC name(params) OPTIONS(...) RETURNS(type); body END;
   Syntax:
     PROC NAME;                           -- no params, no options
     PROC NAME(A INT, B PTR);             -- with params
     PROC NAME OPTIONS(FREESTANDING);     -- with options
     PROC NAME(A INT) RETURNS(INT);       -- with return type
     PROC NAME(A INT) OPTIONS(NAKED) RETURNS(BYTE); body END;

   PROC node layout:
     nd_name  = procedure name
     nd_left  = first parameter (PARAM nodes linked via nd_next)
     nd_right = body (BLOCK node)
     nd_ival  = option flags (OPT_FREESTANDING | OPT_NAKED | OPT_LEAF)
     nd_stor  = return type (TYPE_ constant, TYPE_NONE if no RETURNS) */
int parse_proc(void) {
    int n;
    int nlen;
    char *name;
    int params;
    int ptail;
    int p;
    int opts;
    int ret_type;
    int body;

    if (cur_type != TOK_PROC) {
        parse_error("expected PROC");
        return NODE_NULL;
    }
    parse_advance();

    /* Procedure name -- may be a keyword token (like INIT).
       In label syntax (LABEL: PROC ...) the name follows PROC
       only if the next token is an identifier/keyword name,
       not ( or OPTIONS or RETURNS or ; */
    name = 0;
    if (cur_type != TOK_LPAREN && cur_type != TOK_OPTIONS
        && cur_type != TOK_RETURNS && cur_type != TOK_SEMI) {
        if (!is_alpha(cur_text[0])) {
            parse_error("expected proc name");
            return NODE_NULL;
        }
        nlen = str_len(cur_text);
        name = arena_alloc(nlen + 1);
        if (name) str_copy(name, cur_text);
        parse_advance();
    }

    /* Optional parameter list */
    params = NODE_NULL;
    ptail = NODE_NULL;
    if (cur_type == TOK_LPAREN) {
        parse_advance();
        if (cur_type != TOK_RPAREN) {
            params = parse_param();
            ptail = params;
            while (cur_type == TOK_COMMA && !parse_err) {
                parse_advance();
                p = parse_param();
                if (ptail != NODE_NULL) nd_next[ptail] = p;
                ptail = p;
            }
        }
        parse_expect(TOK_RPAREN);
    }

    /* Optional OPTIONS(keyword, ...) */
    opts = 0;
    if (cur_type == TOK_OPTIONS) {
        parse_advance();
        parse_expect(TOK_LPAREN);
        while (cur_type != TOK_RPAREN && !parse_err) {
            if (cur_type == TOK_FREESTANDING) {
                opts = opts | OPT_FREESTANDING;
                parse_advance();
            } else if (cur_type == TOK_NAKED) {
                opts = opts | OPT_NAKED;
                parse_advance();
            } else if (cur_type == TOK_LEAF) {
                opts = opts | OPT_LEAF;
                parse_advance();
            } else {
                parse_error("unknown OPTION");
                return NODE_NULL;
            }
            if (cur_type == TOK_COMMA) parse_advance();
        }
        parse_expect(TOK_RPAREN);
    }

    /* Optional RETURNS(type) */
    ret_type = TYPE_NONE;
    if (cur_type == TOK_RETURNS) {
        parse_advance();
        parse_expect(TOK_LPAREN);
        ret_type = parse_type();
        if (ret_type == TYPE_NONE) {
            parse_error("expected type in RETURNS");
            return NODE_NULL;
        }
        parse_expect(TOK_RPAREN);
    }

    /* Semicolon after declaration header */
    parse_expect(TOK_SEMI);

    /* Parse body: statements until END */
    body = parse_block();

    /* Expect END and optional proc name after END */
    parse_expect(TOK_END);
    /* Optional name after END (e.g. END foo;) -- skip if identifier */
    if (is_alpha(cur_text[0]) && cur_type != TOK_SEMI) {
        parse_advance();
    }
    parse_expect(TOK_SEMI);

    /* Build PROC node */
    n = nd_alloc(NODE_PROC);
    if (n != NODE_NULL) {
        nd_name[n] = name;
        nd_left[n] = params;
        nd_right[n] = body;
        nd_ival[n] = opts;
        nd_stor[n] = ret_type;
    }
    return n;
}

#endif
