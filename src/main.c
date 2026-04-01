#include "io.h"
#include "arena.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "symtab.h"
#include "types.h"
#include "layout.h"
#include "emit.h"
#include "codegen.h"

#define LINE_MAX 128

void test_strings(void) {
    char a[32];
    char b[32];

    /* test str_len */
    uart_putstr("str_len(\"hello\"): ");
    str_copy(a, "hello");
    print_int(str_len(a));
    uart_putchar(10);

    /* test str_copy */
    str_copy(b, a);
    uart_putstr("str_copy: ");
    uart_puts(b);

    /* test str_ncopy */
    str_ncopy(b, "abcdefghij", 5);
    uart_putstr("str_ncopy(10 chars, max=5): ");
    uart_puts(b);

    /* test str_cmp */
    str_copy(a, "abc");
    str_copy(b, "abc");
    uart_putstr("str_cmp(\"abc\",\"abc\"): ");
    print_int(str_cmp(a, b));
    uart_putchar(10);

    str_copy(b, "abd");
    uart_putstr("str_cmp(\"abc\",\"abd\"): ");
    print_int(str_cmp(a, b));
    uart_putchar(10);

    /* test str_eq */
    str_copy(b, "abc");
    uart_putstr("str_eq(\"abc\",\"abc\"): ");
    print_int(str_eq(a, b));
    uart_putchar(10);

    str_copy(b, "xyz");
    uart_putstr("str_eq(\"abc\",\"xyz\"): ");
    print_int(str_eq(a, b));
    uart_putchar(10);

    /* test mem_set and mem_copy */
    mem_set(a, 65, 5);  /* fill with 'A' */
    a[5] = 0;
    uart_putstr("mem_set(A,5): ");
    uart_puts(a);

    mem_copy(b, a, 6);  /* include null */
    uart_putstr("mem_copy: ");
    uart_puts(b);
}

void test_arena(void) {
    arena_init();

    uart_putstr("arena free: ");
    print_int(arena_free());
    uart_putchar(10);

    /* allocate a string */
    char *s1 = arena_alloc(16);
    if (s1) {
        str_copy(s1, "arena-string-1");
        uart_putstr("alloc 16: ");
        uart_puts(s1);
    }

    /* allocate another */
    char *s2 = arena_alloc(16);
    if (s2) {
        str_copy(s2, "arena-string-2");
        uart_putstr("alloc 16: ");
        uart_puts(s2);
    }

    /* check both still valid */
    uart_putstr("s1: ");
    uart_puts(s1);
    uart_putstr("s2: ");
    uart_puts(s2);

    uart_putstr("arena free: ");
    print_int(arena_free());
    uart_putchar(10);

    /* test save/restore */
    int mark = arena_save();
    char *s3 = arena_alloc(32);
    if (s3) {
        str_copy(s3, "temporary");
    }
    uart_putstr("after temp alloc, free: ");
    print_int(arena_free());
    uart_putchar(10);

    arena_restore(mark);
    uart_putstr("after restore, free: ");
    print_int(arena_free());
    uart_putchar(10);
}

void test_kw(char *word, int expect_kw) {
    int type = kw_lookup(word);
    uart_putstr("  ");
    uart_putstr(word);
    uart_putstr(" -> ");
    uart_putstr(tok_name(type));
    if (expect_kw && type == TOK_IDENT) {
        uart_putstr(" FAIL(expected keyword)");
    }
    if (!expect_kw && type != TOK_IDENT) {
        uart_putstr(" FAIL(expected IDENT)");
    }
    uart_putchar(10);
}

void test_tokens(void) {
    kw_init();

    uart_puts("keywords (should match):");
    test_kw("DCL", 1);
    test_kw("DECLARE", 1);
    test_kw("PROC", 1);
    test_kw("IF", 1);
    test_kw("THEN", 1);
    test_kw("ELSE", 1);
    test_kw("DO", 1);
    test_kw("WHILE", 1);
    test_kw("END", 1);
    test_kw("CALL", 1);
    test_kw("RETURN", 1);
    test_kw("ASM", 1);
    test_kw("STATIC", 1);
    test_kw("AUTOMATIC", 1);
    test_kw("EXTERNAL", 1);
    test_kw("INT", 1);
    test_kw("BYTE", 1);
    test_kw("CHAR", 1);
    test_kw("PTR", 1);
    test_kw("WORD", 1);
    test_kw("BIT", 1);
    test_kw("OPTIONS", 1);
    test_kw("RETURNS", 1);
    test_kw("TO", 1);
    test_kw("ADDR", 1);
    test_kw("BASED", 1);
    test_kw("NAKED", 1);

    uart_puts("case insensitive (should match):");
    test_kw("dcl", 1);
    test_kw("Proc", 1);
    test_kw("rEtUrN", 1);

    uart_puts("identifiers (should NOT match):");
    test_kw("myvar", 0);
    test_kw("counter", 0);
    test_kw("HELLO", 0);
    test_kw("x", 0);

    uart_puts("tok_name spot check:");
    uart_putstr("  TOK_PLUS=");
    uart_puts(tok_name(TOK_PLUS));
    uart_putstr("  TOK_SEMI=");
    uart_puts(tok_name(TOK_SEMI));
    uart_putstr("  TOK_EOF=");
    uart_puts(tok_name(TOK_EOF));
    uart_putstr("  TOK_NE=");
    uart_puts(tok_name(TOK_NE));
}

void test_lexer(void) {
    /* Test 1: DCL statement */
    lex_dump("DCL stmt",
        "DCL counter INT;");

    /* Test 2: procedure with body */
    lex_dump("PROC",
        "foo: PROC(x, y) RETURNS(INT); RETURN(x + y); END foo;");

    /* Test 3: control flow */
    lex_dump("IF/DO",
        "IF count > 0 THEN DO; CALL print(count); count = count - 1; END;");

    /* Test 4: multi-char operators */
    lex_dump("operators",
        "a <= b != c >= d << 2 >> 1 ptr->field");

    /* Test 5: numbers and strings */
    lex_dump("literals",
        "x = 42; msg = 'hello world';");

    /* Test 6: comments stripped */
    lex_dump("comments",
        "DCL /* this is a comment */ x INT;");

    /* Test 7: case insensitive keywords */
    lex_dump("case",
        "dcl Proc Return");

    /* Test 8: macro syntax */
    lex_dump("macro",
        "?GETMAIN(LENGTH(256), TYPE(BYTE))");

    /* Test 9: mixed punctuation */
    lex_dump("punct",
        "a.b, c(d); ~e & f | g ^ h");

    /* Test 10: percent directive */
    lex_dump("directive",
        "%INCLUDE SYSLIB;");
}

void test_ast(void) {
    arena_init();
    ast_init();

    /* Test 1: basic allocation */
    uart_puts("alloc nodes:");
    int prog = nd_alloc(NODE_PROGRAM);
    uart_putstr("  program node idx=");
    print_int(prog);
    uart_putstr(" kind=");
    uart_puts(nd_kind_name(nd_kind[prog]));

    /* Test 2: literal and ident nodes */
    int lit = nd_literal(42);
    uart_putstr("  literal idx=");
    print_int(lit);
    uart_putstr(" val=");
    print_int(nd_ival[lit]);
    uart_putchar(10);

    int id = nd_ident("counter");
    uart_putstr("  ident idx=");
    print_int(id);
    uart_putstr(" name=");
    uart_puts(nd_name[id]);

    /* Test 3: binary op tree: counter + 42 */
    int add = nd_binop(TOK_PLUS, id, lit);
    uart_putstr("  binop idx=");
    print_int(add);
    uart_putstr(" op=");
    uart_putstr(tok_name(nd_ival[add]));
    uart_putstr(" left=");
    print_int(nd_left[add]);
    uart_putstr(" right=");
    print_int(nd_right[add]);
    uart_putchar(10);

    /* Test 4: assignment: x = counter + 42 */
    int x = nd_ident("x");
    int asgn = nd_assign(x, add);
    uart_putstr("  assign idx=");
    print_int(asgn);
    uart_putstr(" target=");
    print_int(nd_left[asgn]);
    uart_putstr(" value=");
    print_int(nd_right[asgn]);
    uart_putchar(10);

    /* Test 5: unary op: ~x */
    int neg = nd_unop(TOK_TILDE, x);
    uart_putstr("  unop idx=");
    print_int(neg);
    uart_putstr(" op=");
    uart_putstr(tok_name(nd_ival[neg]));
    uart_putchar(10);

    /* Test 6: call node: print(x) */
    int call = nd_call("print", x);
    uart_putstr("  call idx=");
    print_int(call);
    uart_putstr(" name=");
    uart_putstr(nd_name[call]);
    uart_putstr(" args=");
    print_int(nd_left[call]);
    uart_putchar(10);

    /* Test 7: return node */
    int ret = nd_return(lit);
    uart_putstr("  return idx=");
    print_int(ret);
    uart_putstr(" expr=");
    print_int(nd_left[ret]);
    uart_putchar(10);

    /* Test 8: append children to program */
    nd_append(prog, asgn);
    nd_append(prog, call);
    nd_append(prog, ret);
    uart_putstr("  program children: ");
    print_int(nd_left[prog]);
    uart_putstr(" -> ");
    print_int(nd_next[nd_left[prog]]);
    uart_putstr(" -> ");
    print_int(nd_next[nd_next[nd_left[prog]]]);
    uart_putchar(10);

    /* Test 9: pool count */
    uart_putstr("  pool used: ");
    print_int(nd_count);
    uart_putstr("/");
    print_int(NODE_POOL_MAX);
    uart_putchar(10);

    /* Test 10: tree dump */
    uart_puts("tree dump:");
    nd_dump(prog, 0);

    /* Test 11: DCL node with type info */
    int dcl = nd_alloc(NODE_DCL);
    nd_set_name(dcl, "buffer");
    nd_type[dcl] = TYPE_BYTE;
    nd_stor[dcl] = STOR_STATIC;
    nd_level[dcl] = 1;
    uart_putstr("  dcl name=");
    uart_putstr(nd_name[dcl]);
    uart_putstr(" type=");
    uart_putstr(nd_type_name(nd_type[dcl]));
    uart_putstr(" level=");
    print_int(nd_level[dcl]);
    uart_putchar(10);
}

void test_parse_dcl(char *label, char *src) {
    uart_putstr("--- ");
    uart_putstr(label);
    uart_puts(" ---");
    uart_putstr("  src: ");
    uart_puts(src);

    arena_init();
    ast_init();
    parse_init(src);

    int n = parse_dcl();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
    } else if (n == NODE_NULL) {
        uart_puts("  (null node)");
    } else {
        nd_dump(n, 1);
    }
    uart_putchar(10);
}

void test_parser(void) {
    /* Test 1: simple scalar */
    test_parse_dcl("scalar INT(24)",
        "DCL COUNT INT(24);");

    /* Test 2: BYTE type */
    test_parse_dcl("scalar BYTE",
        "DCL FLAGS BYTE;");

    /* Test 3: CHAR type */
    test_parse_dcl("scalar CHAR",
        "DCL LETTER CHAR;");

    /* Test 4: PTR type */
    test_parse_dcl("scalar PTR",
        "DCL BUFPTR PTR;");

    /* Test 5: WORD type */
    test_parse_dcl("scalar WORD",
        "DCL STATUS WORD;");

    /* Test 6: BIT type */
    test_parse_dcl("scalar BIT",
        "DCL FLAG BIT;");

    /* Test 7: INT default (no width) */
    test_parse_dcl("INT no width",
        "DCL X INT;");

    /* Test 8: INT(8) */
    test_parse_dcl("INT(8)",
        "DCL SMALL INT(8);");

    /* Test 9: INT(16) */
    test_parse_dcl("INT(16)",
        "DCL MEDIUM INT(16);");

    /* Test 10: array declaration */
    test_parse_dcl("array CHAR",
        "DCL BUFFER(80) CHAR;");

    /* Test 11: array with BYTE */
    test_parse_dcl("array BYTE",
        "DCL DATA(256) BYTE;");

    /* Test 12: STATIC storage */
    test_parse_dcl("STATIC",
        "DCL COUNTER INT(24) STATIC;");

    /* Test 13: EXTERNAL storage */
    test_parse_dcl("EXTERNAL",
        "DCL EXTERN_SYM INT(24) EXTERNAL;");

    /* Test 14: INIT with number */
    test_parse_dcl("INIT num",
        "DCL START INT(24) STATIC INIT(0);");

    /* Test 15: INIT with string */
    test_parse_dcl("INIT string",
        "DCL MSG(18) CHAR STATIC INIT('Hello from PL/SW!');");

    /* Test 16: DECLARE (long form) */
    test_parse_dcl("DECLARE",
        "DECLARE TOTAL INT(24);");

    /* Test 17: level-based record */
    test_parse_dcl("record",
        "DCL 1 POINT, 3 X INT(24), 3 Y INT(24);");

    /* Test 18: record with mixed types */
    test_parse_dcl("record mixed",
        "DCL 1 DEVICE, 3 ID BYTE, 3 NAME(8) CHAR, 3 STATUS WORD;");

    /* Test 19: record with storage class */
    test_parse_dcl("record STATIC",
        "DCL 1 CONFIG STATIC, 3 BAUD INT(24), 3 PARITY BYTE;");
}

void test_parse_expr(char *label, char *src) {
    uart_putstr("--- ");
    uart_putstr(label);
    uart_puts(" ---");
    uart_putstr("  src: ");
    uart_puts(src);

    arena_init();
    ast_init();
    parse_init(src);

    int n = parse_expr();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
    } else if (n == NODE_NULL) {
        uart_puts("  (null node)");
    } else {
        nd_dump(n, 1);
    }
    uart_putchar(10);
}

void test_expr_parser(void) {
    /* Test 1: simple literal */
    test_parse_expr("literal", "42");

    /* Test 2: identifier */
    test_parse_expr("ident", "x");

    /* Test 3: binary add */
    test_parse_expr("add", "x + 1");

    /* Test 4: binary sub */
    test_parse_expr("sub", "a - b");

    /* Test 5: multiply */
    test_parse_expr("mul", "a * b");

    /* Test 6: divide */
    test_parse_expr("div", "a / b");

    /* Test 7: precedence: a + b * c  =>  a + (b*c) */
    test_parse_expr("prec add*mul", "a + b * c");

    /* Test 8: left associativity: a - b - c => (a-b)-c */
    test_parse_expr("left assoc", "a - b - c");

    /* Test 9: parenthesized: (a + b) * c */
    test_parse_expr("parens", "(a + b) * c");

    /* Test 10: unary minus */
    test_parse_expr("unary -", "-x");

    /* Test 11: bitwise NOT */
    test_parse_expr("unary ~", "~flags");

    /* Test 12: logical NOT */
    test_parse_expr("NOT", "NOT done");

    /* Test 13: comparison operators */
    test_parse_expr("cmp <", "a < b");
    test_parse_expr("cmp >=", "x >= 10");
    test_parse_expr("cmp !=", "a != 0");

    /* Test 14: equality */
    test_parse_expr("eq =", "x = 0");

    /* Test 15: shift operators */
    test_parse_expr("shl", "x << 2");
    test_parse_expr("shr", "x >> 1");

    /* Test 16: bitwise AND/OR/XOR */
    test_parse_expr("bw and", "a & b");
    test_parse_expr("bw or", "a | b");
    test_parse_expr("bw xor", "a ^ b");

    /* Test 17: logical AND/OR */
    test_parse_expr("log and", "a AND b");
    test_parse_expr("log or", "a OR b");

    /* Test 18: complex precedence: a OR b AND c = d */
    test_parse_expr("complex prec", "a OR b AND c = d");

    /* Test 19: function call */
    test_parse_expr("call 0 args", "foo()");
    test_parse_expr("call 1 arg", "print(x)");
    test_parse_expr("call 2 args", "add(x, y)");
    test_parse_expr("call 3 args", "f(a, b, c)");

    /* Test 20: nested calls */
    test_parse_expr("nested call", "f(g(x))");

    /* Test 21: call in expression */
    test_parse_expr("call in expr", "f(x) + g(y)");

    /* Test 22: ADDR */
    test_parse_expr("ADDR", "ADDR(buffer)");

    /* Test 23: field access */
    test_parse_expr("field", "point.x");

    /* Test 24: chained field */
    test_parse_expr("chain field", "a.b.c");

    /* Test 25: pointer dereference */
    test_parse_expr("deref", "p->field");

    /* Test 26: complex expression with all features */
    test_parse_expr("complex",
        "(a + b) * c - f(x, y) + point.z");

    /* Test 27: unary in binary */
    test_parse_expr("unary in bin", "-a + b");

    /* Test 28: nested parens */
    test_parse_expr("nested parens", "((a + b))");

    /* Test 29: shift with add */
    test_parse_expr("shift+add", "a + b << 2");

    /* Test 30: bitwise precedence chain */
    test_parse_expr("bw chain", "a & b | c ^ d");
}

void test_parse_stmt(char *label, char *src) {
    uart_putstr("--- ");
    uart_putstr(label);
    uart_puts(" ---");
    uart_putstr("  src: ");
    uart_puts(src);

    arena_init();
    ast_init();
    parse_init(src);

    int n = parse_stmt();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
    } else if (n == NODE_NULL) {
        uart_puts("  (null node)");
    } else {
        nd_dump(n, 1);
    }
    uart_putchar(10);
}

void test_parse_block(char *label, char *src) {
    uart_putstr("--- ");
    uart_putstr(label);
    uart_puts(" ---");
    uart_putstr("  src: ");
    uart_puts(src);

    arena_init();
    ast_init();
    parse_init(src);

    /* Parse DO; ... END; as a block */
    int n = parse_stmt();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
    } else if (n == NODE_NULL) {
        uart_puts("  (null node)");
    } else {
        nd_dump(n, 1);
    }
    uart_putchar(10);
}

void test_stmt_parser(void) {
    /* Test 1: simple assignment */
    test_parse_stmt("assign",
        "x = 42;");

    /* Test 2: assignment with expression */
    test_parse_stmt("assign expr",
        "x = a + b * c;");

    /* Test 3: field assignment */
    test_parse_stmt("field assign",
        "rec.x = 10;");

    /* Test 4: IF THEN */
    test_parse_stmt("IF THEN",
        "IF x = 0 THEN y = 1;");

    /* Test 5: IF THEN ELSE */
    test_parse_stmt("IF THEN ELSE",
        "IF x > 0 THEN y = 1; ELSE y = 0;");

    /* Test 6: IF THEN DO...END */
    test_parse_stmt("IF THEN DO",
        "IF x > 0 THEN DO; y = 1; z = 2; END;");

    /* Test 7: IF THEN DO ELSE DO */
    test_parse_stmt("IF ELSE DO",
        "IF x THEN DO; a = 1; END; ELSE DO; b = 2; END;");

    /* Test 8: DO WHILE */
    test_parse_stmt("DO WHILE",
        "DO WHILE (x > 0); x = x - 1; END;");

    /* Test 9: DO count */
    test_parse_stmt("DO count",
        "DO I = 1 TO 10; CALL print(I); END;");

    /* Test 10: CALL no args */
    test_parse_stmt("CALL no args",
        "CALL init();");

    /* Test 11: CALL with args */
    test_parse_stmt("CALL args",
        "CALL print(42);");

    /* Test 12: CALL multiple args */
    test_parse_stmt("CALL multi",
        "CALL move(x, y, z);");

    /* Test 13: RETURN with expr */
    test_parse_stmt("RETURN expr",
        "RETURN(x + 1);");

    /* Test 14: RETURN bare */
    test_parse_stmt("RETURN bare",
        "RETURN;");

    /* Test 15: DO block */
    test_parse_block("DO block",
        "DO; x = 1; y = 2; z = 3; END;");

    /* Test 16: nested IF */
    test_parse_stmt("nested IF",
        "IF a THEN IF b THEN x = 1;");

    /* Test 17: compound block with mixed statements */
    test_parse_block("compound",
        "DO; DCL x INT; x = 0; IF x = 0 THEN CALL init(); RETURN(x); END;");

    /* Test 18: DO WHILE with multiple stmts */
    test_parse_stmt("DO WHILE multi",
        "DO WHILE (n > 0); sum = sum + n; n = n - 1; END;");

    /* Test 19: array subscript assign */
    test_parse_stmt("array assign",
        "buf(i) = 65;");

    /* Test 20: nested DO blocks */
    test_parse_stmt("nested DO",
        "DO WHILE (1); DO WHILE (0); x = 1; END; END;");
}

void test_parse_proc(char *label, char *src) {
    uart_putstr("--- ");
    uart_putstr(label);
    uart_puts(" ---");
    uart_putstr("  src: ");
    uart_puts(src);

    arena_init();
    ast_init();
    parse_init(src);

    int n = parse_proc();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
    } else if (n == NODE_NULL) {
        uart_puts("  (null node)");
    } else {
        nd_dump(n, 1);
    }
    uart_putchar(10);
}

void test_proc_parser(void) {
    /* Test 1: minimal procedure -- no params, no options */
    test_parse_proc("minimal",
        "PROC FOO; RETURN; END;");

    /* Test 2: one parameter */
    test_parse_proc("one param",
        "PROC SQUARE(X INT(24)) RETURNS(INT(24)); RETURN(X * X); END;");

    /* Test 3: two parameters */
    test_parse_proc("two params",
        "PROC ADD(A INT(24), B INT(24)) RETURNS(INT(24)); RETURN(A + B); END;");

    /* Test 4: three params, mixed types */
    test_parse_proc("mixed params",
        "PROC FILL(BUF PTR, VAL BYTE, LEN INT(24)); END;");

    /* Test 5: OPTIONS(FREESTANDING) */
    test_parse_proc("FREESTANDING",
        "PROC MAIN OPTIONS(FREESTANDING); CALL INIT(); END;");

    /* Test 6: OPTIONS(NAKED) */
    test_parse_proc("NAKED",
        "PROC IRQ OPTIONS(NAKED); END;");

    /* Test 7: OPTIONS(LEAF) */
    test_parse_proc("LEAF",
        "PROC GETVAL OPTIONS(LEAF) RETURNS(INT(24)); RETURN(42); END;");

    /* Test 8: multiple options */
    test_parse_proc("multi opts",
        "PROC HANDLER OPTIONS(NAKED, LEAF); END;");

    /* Test 9: body with DCL and statements */
    test_parse_proc("body DCL+stmts",
        "PROC COMPUTE(X INT(24)) RETURNS(INT(24)); DCL RESULT INT(24); RESULT = X * 2 + 1; RETURN(RESULT); END;");

    /* Test 10: body with IF */
    test_parse_proc("body IF",
        "PROC ABS(X INT(24)) RETURNS(INT(24)); IF X < 0 THEN RETURN(-X); RETURN(X); END;");

    /* Test 11: body with DO WHILE loop */
    test_parse_proc("body loop",
        "PROC COUNTDOWN(N INT(24)); DO WHILE (N > 0); CALL PRINT(N); N = N - 1; END; END;");

    /* Test 12: END with proc name */
    test_parse_proc("END name",
        "PROC FOO; RETURN; END FOO;");

    /* Test 13: empty param list () */
    test_parse_proc("empty parens",
        "PROC NOOP(); END;");

    /* Test 14: CHAR param */
    test_parse_proc("CHAR param",
        "PROC PUTC(CH CHAR); END;");

    /* Test 15: complex realistic procedure */
    test_parse_proc("realistic",
        "PROC UART_PUTS(S PTR); DCL I INT(24); DCL CH CHAR; I = 0; DO WHILE (1); CALL PUTC(42); I = I + 1; END; END;");
}

void test_parse_program(char *label, char *src) {
    uart_putstr("--- ");
    uart_putstr(label);
    uart_puts(" ---");
    uart_putstr("  src: ");
    uart_puts(src);

    arena_init();
    ast_init();
    parse_init(src);

    int n = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
    } else if (n == NODE_NULL) {
        uart_puts("  (null node)");
    } else {
        nd_dump(n, 1);
    }
    uart_putchar(10);
}

void test_toplevel_parser(void) {
    /* Test 1: single global DCL */
    test_parse_program("single DCL",
        "DCL COUNT INT(24);");

    /* Test 2: multiple global DCLs */
    test_parse_program("multi DCL",
        "DCL X INT(24); DCL Y INT(24); DCL Z BYTE;");

    /* Test 3: single PROC */
    test_parse_program("single PROC",
        "PROC FOO; RETURN; END;");

    /* Test 4: DCL then PROC */
    test_parse_program("DCL+PROC",
        "DCL TOTAL INT(24); PROC ADD(A INT(24), B INT(24)) RETURNS(INT(24)); TOTAL = A + B; RETURN(TOTAL); END;");

    /* Test 5: PROC then DCL (DCLs can follow PROCs at top level) */
    test_parse_program("PROC+DCL",
        "PROC INIT; RETURN; END; DCL STATUS BYTE;");

    /* Test 6: multiple PROCs */
    test_parse_program("multi PROC",
        "PROC FIRST; RETURN; END; PROC SECOND; RETURN; END;");

    /* Test 7: label: PROC syntax */
    test_parse_program("label PROC",
        "MAIN: PROC OPTIONS(FREESTANDING); CALL INIT(); END;");

    /* Test 8: full program with globals, helpers, and main */
    test_parse_program("full program",
        "DCL COUNTER INT(24) STATIC INIT(0); DCL LIMIT INT(24) INIT(10); PROC INCR; COUNTER = COUNTER + 1; END; PROC MAIN OPTIONS(FREESTANDING); DO WHILE (COUNTER < LIMIT); CALL INCR(); END; END;");

    /* Test 9: PROC with RETURNS and local DCLs */
    test_parse_program("PROC locals",
        "PROC SQUARE(N INT(24)) RETURNS(INT(24)); DCL R INT(24); R = N * N; RETURN(R); END;");

    /* Test 10: record DCL + PROC that uses it */
    test_parse_program("record+PROC",
        "DCL 1 POINT, 3 X INT(24), 3 Y INT(24); PROC DIST(P PTR) RETURNS(INT(24)); RETURN(P->X + P->Y); END;");

    /* Test 11: empty program */
    test_parse_program("empty", "");

    /* Test 12: DECLARE (long form) at top level */
    test_parse_program("DECLARE",
        "DECLARE BUF(80) CHAR; PROC FILL; END;");
}

void test_symtab(void) {
    int idx;

    /* Test 1: init and insert at global scope */
    uart_puts("--- global scope ---");
    arena_init();
    sym_init();

    idx = sym_insert("COUNT", TYPE_INT24, 3, STOR_AUTO);
    uart_putstr("insert COUNT: idx=");
    print_int(idx);
    uart_putchar(10);
    sym_print(idx);

    idx = sym_insert("FLAGS", TYPE_BYTE, 1, STOR_STATIC);
    uart_putstr("insert FLAGS: idx=");
    print_int(idx);
    uart_putchar(10);
    sym_print(idx);

    idx = sym_insert("BUFPTR", TYPE_PTR, 3, STOR_AUTO);
    uart_putstr("insert BUFPTR: idx=");
    print_int(idx);
    uart_putchar(10);

    uart_putstr("global scope size: ");
    print_int(sym_scope_size());
    uart_putchar(10);

    /* Test 2: lookup in global scope */
    uart_puts("--- lookup global ---");
    idx = sym_lookup("COUNT");
    uart_putstr("lookup COUNT: ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup("FLAGS");
    uart_putstr("lookup FLAGS: ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup("MISSING");
    uart_putstr("lookup MISSING: ");
    print_int(idx);
    uart_putchar(10);

    /* Test 3: case insensitive lookup */
    uart_puts("--- case insensitive ---");
    idx = sym_lookup("count");
    uart_putstr("lookup count: ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup("Flags");
    uart_putstr("lookup Flags: ");
    print_int(idx);
    uart_putchar(10);

    /* Test 4: enter procedure scope */
    uart_puts("--- proc scope ---");
    sym_enter_scope();
    uart_putstr("depth after enter: ");
    print_int(sym_get_depth());
    uart_putchar(10);

    idx = sym_insert("X", TYPE_INT24, 3, STOR_AUTO);
    sym_flags[idx] = SYM_F_PARAM;
    uart_putstr("insert X (param): idx=");
    print_int(idx);
    uart_putchar(10);

    idx = sym_insert("Y", TYPE_INT24, 3, STOR_AUTO);
    sym_flags[idx] = SYM_F_PARAM;
    uart_putstr("insert Y (param): idx=");
    print_int(idx);
    uart_putchar(10);

    idx = sym_insert("LOCAL", TYPE_BYTE, 1, STOR_AUTO);
    uart_putstr("insert LOCAL: idx=");
    print_int(idx);
    uart_putchar(10);

    /* Test 5: lookup from inner scope finds outer */
    uart_puts("--- inner sees outer ---");
    idx = sym_lookup("COUNT");
    uart_putstr("lookup COUNT (from proc): ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup("LOCAL");
    uart_putstr("lookup LOCAL: ");
    print_int(idx);
    uart_putchar(10);

    /* Test 6: shadowing */
    uart_puts("--- shadowing ---");
    idx = sym_insert("COUNT", TYPE_INT16, 2, STOR_AUTO);
    uart_putstr("insert COUNT (shadow): idx=");
    print_int(idx);
    uart_putchar(10);

    /* Lookup should find the inner (shadowed) one */
    idx = sym_lookup("COUNT");
    uart_putstr("lookup COUNT (should be inner): idx=");
    print_int(idx);
    uart_putstr(" type=");
    uart_puts(nd_type_name(sym_type[idx]));

    /* Test 7: local-only lookup */
    uart_puts("--- local only ---");
    idx = sym_lookup_local("COUNT");
    uart_putstr("local COUNT: ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup_local("FLAGS");
    uart_putstr("local FLAGS (global only): ");
    print_int(idx);
    uart_putchar(10);

    /* Test 8: dump current scope */
    uart_puts("--- dump proc scope ---");
    sym_dump_scope();

    /* Test 9: exit scope, verify outer restored */
    uart_puts("--- exit proc scope ---");
    sym_exit_scope();
    uart_putstr("depth after exit: ");
    print_int(sym_get_depth());
    uart_putchar(10);

    /* COUNT should resolve to global again */
    idx = sym_lookup("COUNT");
    uart_putstr("lookup COUNT (should be global): idx=");
    print_int(idx);
    uart_putstr(" type=");
    uart_puts(nd_type_name(sym_type[idx]));

    /* LOCAL should not be found */
    idx = sym_lookup("LOCAL");
    uart_putstr("lookup LOCAL (should fail): ");
    print_int(idx);
    uart_putchar(10);

    /* Test 10: duplicate detection */
    uart_puts("--- duplicate ---");
    sym_err = 0;
    idx = sym_insert("COUNT", TYPE_INT24, 3, STOR_AUTO);
    uart_putstr("insert COUNT again: idx=");
    print_int(idx);
    if (sym_err) {
        uart_putstr(" err=");
        uart_puts(sym_errmsg);
    } else {
        uart_putchar(10);
    }

    /* Test 11: nested scopes (2 levels deep) */
    uart_puts("--- nested scopes ---");
    sym_enter_scope();
    sym_insert("A", TYPE_INT24, 3, STOR_AUTO);
    sym_enter_scope();
    sym_insert("B", TYPE_BYTE, 1, STOR_AUTO);

    uart_putstr("depth: ");
    print_int(sym_get_depth());
    uart_putchar(10);

    idx = sym_lookup("B");
    uart_putstr("lookup B: ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup("A");
    uart_putstr("lookup A: ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup("COUNT");
    uart_putstr("lookup COUNT (global from depth 2): ");
    print_int(idx);
    uart_putchar(10);

    sym_exit_scope();
    idx = sym_lookup("B");
    uart_putstr("lookup B after exit: ");
    print_int(idx);
    uart_putchar(10);

    idx = sym_lookup("A");
    uart_putstr("lookup A after exit: ");
    print_int(idx);
    uart_putchar(10);

    sym_exit_scope();

    /* Test 12: dump all scopes */
    uart_puts("--- dump all ---");
    sym_dump_all();

    /* Test 13: procedure symbol */
    uart_puts("--- proc symbol ---");
    idx = sym_insert("MAIN", TYPE_NONE, 0, STOR_AUTO);
    sym_flags[idx] = SYM_F_PROC;
    sym_print(idx);

    /* Test 14: array symbol */
    idx = sym_insert("BUFFER", TYPE_CHAR, 80, STOR_STATIC);
    sym_flags[idx] = SYM_F_ARRAY;
    sym_print(idx);

    /* Test 15: symbol with offset */
    sym_enter_scope();
    idx = sym_insert("LOCALVAR", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[idx] = -3;
    sym_print(idx);
    sym_exit_scope();
}

void test_type_compat(char *label, int from, int to, int expect) {
    uart_putstr("  ");
    uart_putstr(label);
    uart_putstr(": ");
    uart_putstr(nd_type_name(from));
    uart_putstr(" -> ");
    uart_putstr(nd_type_name(to));
    tc_err = 0;
    int ok = type_compat_assign(from, to);
    if (ok == expect) {
        uart_puts(" OK");
    } else {
        uart_puts(" FAIL");
    }
}

void test_type_binop(char *label, int op, int lt, int rt, int expect) {
    uart_putstr("  ");
    uart_putstr(label);
    uart_putstr(": ");
    uart_putstr(nd_type_name(lt));
    uart_putstr(" ");
    uart_putstr(tok_name(op));
    uart_putstr(" ");
    uart_putstr(nd_type_name(rt));
    tc_err = 0;
    int result = type_binop_result(op, lt, rt);
    uart_putstr(" => ");
    uart_putstr(nd_type_name(result));
    if (result == expect) {
        uart_puts(" OK");
    } else {
        uart_putstr(" FAIL(expected ");
        uart_putstr(nd_type_name(expect));
        uart_puts(")");
    }
}

void test_types(void) {
    int desc;
    int fidx;
    int rec;

    /* Test 1: type widths */
    uart_puts("--- type widths ---");
    uart_putstr("  BIT="); print_int(type_width(TYPE_BIT)); uart_putchar(10);
    uart_putstr("  INT8="); print_int(type_width(TYPE_INT8)); uart_putchar(10);
    uart_putstr("  BYTE="); print_int(type_width(TYPE_BYTE)); uart_putchar(10);
    uart_putstr("  CHAR="); print_int(type_width(TYPE_CHAR)); uart_putchar(10);
    uart_putstr("  INT16="); print_int(type_width(TYPE_INT16)); uart_putchar(10);
    uart_putstr("  WORD="); print_int(type_width(TYPE_WORD)); uart_putchar(10);
    uart_putstr("  INT24="); print_int(type_width(TYPE_INT24)); uart_putchar(10);
    uart_putstr("  PTR="); print_int(type_width(TYPE_PTR)); uart_putchar(10);

    /* Test 2: type classification */
    uart_puts("--- type classification ---");
    uart_putstr("  INT24 is_integer: "); print_int(type_is_integer(TYPE_INT24)); uart_putchar(10);
    uart_putstr("  PTR is_integer: "); print_int(type_is_integer(TYPE_PTR)); uart_putchar(10);
    uart_putstr("  INT24 is_signed: "); print_int(type_is_signed(TYPE_INT24)); uart_putchar(10);
    uart_putstr("  BYTE is_signed: "); print_int(type_is_signed(TYPE_BYTE)); uart_putchar(10);
    uart_putstr("  PTR is_ptr: "); print_int(type_is_ptr(TYPE_PTR)); uart_putchar(10);
    uart_putstr("  INT24 is_ptr: "); print_int(type_is_ptr(TYPE_INT24)); uart_putchar(10);
    uart_putstr("  INT24 is_scalar: "); print_int(type_is_scalar(TYPE_INT24)); uart_putchar(10);
    uart_putstr("  PTR is_scalar: "); print_int(type_is_scalar(TYPE_PTR)); uart_putchar(10);
    uart_putstr("  RECORD is_scalar: "); print_int(type_is_scalar(TYPE_RECORD)); uart_putchar(10);

    /* Test 3: assignment compatibility */
    uart_puts("--- assignment compat ---");
    test_type_compat("same", TYPE_INT24, TYPE_INT24, 1);
    test_type_compat("int->int", TYPE_INT8, TYPE_INT24, 1);
    test_type_compat("int->byte", TYPE_INT24, TYPE_BYTE, 1);
    test_type_compat("byte->char", TYPE_BYTE, TYPE_CHAR, 1);
    test_type_compat("int->ptr", TYPE_INT24, TYPE_PTR, 1);
    test_type_compat("ptr->int", TYPE_PTR, TYPE_INT24, 1);
    test_type_compat("ptr->ptr", TYPE_PTR, TYPE_PTR, 1);
    test_type_compat("none->int", TYPE_NONE, TYPE_INT24, 1);
    test_type_compat("rec->int", TYPE_RECORD, TYPE_INT24, 0);
    test_type_compat("arr->int", TYPE_ARRAY, TYPE_INT24, 0);

    /* Test 4: binary operator result types */
    uart_puts("--- binop results ---");
    test_type_binop("add ints", TOK_PLUS, TYPE_INT24, TYPE_INT24, TYPE_INT24);
    test_type_binop("add mixed", TOK_PLUS, TYPE_INT8, TYPE_INT24, TYPE_INT24);
    test_type_binop("sub bytes", TOK_MINUS, TYPE_BYTE, TYPE_BYTE, TYPE_BYTE);
    test_type_binop("mul int16", TOK_STAR, TYPE_INT16, TYPE_INT16, TYPE_INT16);
    test_type_binop("cmp eq", TOK_EQ, TYPE_INT24, TYPE_INT24, TYPE_INT24);
    test_type_binop("cmp lt", TOK_LT, TYPE_INT8, TYPE_INT24, TYPE_INT24);
    test_type_binop("ptr+int", TOK_PLUS, TYPE_PTR, TYPE_INT24, TYPE_PTR);
    test_type_binop("int+ptr", TOK_PLUS, TYPE_INT24, TYPE_PTR, TYPE_PTR);
    test_type_binop("ptr-int", TOK_MINUS, TYPE_PTR, TYPE_INT24, TYPE_PTR);
    test_type_binop("ptr-ptr", TOK_MINUS, TYPE_PTR, TYPE_PTR, TYPE_INT24);
    test_type_binop("and", TOK_AND, TYPE_INT24, TYPE_INT24, TYPE_INT24);
    test_type_binop("or", TOK_OR, TYPE_INT24, TYPE_INT24, TYPE_INT24);
    test_type_binop("bw and", TOK_AMP, TYPE_BYTE, TYPE_BYTE, TYPE_BYTE);
    test_type_binop("shift", TOK_SHL, TYPE_INT24, TYPE_INT8, TYPE_INT24);

    /* Test 5: unary operator result types */
    uart_puts("--- unop results ---");
    tc_err = 0;
    uart_putstr("  neg INT24: ");
    uart_puts(nd_type_name(type_unop_result(TOK_MINUS, TYPE_INT24)));
    tc_err = 0;
    uart_putstr("  not BYTE: ");
    uart_puts(nd_type_name(type_unop_result(TOK_TILDE, TYPE_BYTE)));
    tc_err = 0;
    uart_putstr("  NOT INT24: ");
    uart_puts(nd_type_name(type_unop_result(TOK_NOT, TYPE_INT24)));

    /* Test 6: array type descriptor */
    uart_puts("--- array descriptor ---");
    types_init();
    desc = td_make_array(TYPE_BYTE, 80);
    uart_putstr("  array desc idx=");
    print_int(desc);
    uart_putchar(10);
    td_print(desc);

    desc = td_make_array(TYPE_INT24, 10);
    uart_putstr("  array desc idx=");
    print_int(desc);
    uart_putchar(10);
    td_print(desc);

    /* Test 7: record type descriptor from AST */
    uart_puts("--- record descriptor ---");
    arena_init();
    ast_init();
    parse_init("DCL 1 POINT, 3 X INT(24), 3 Y INT(24);");
    rec = parse_dcl();
    if (!parse_err && rec != NODE_NULL) {
        desc = td_make_record(rec);
        uart_putstr("  record desc idx=");
        print_int(desc);
        uart_putchar(10);
        td_print(desc);

        /* Test 8: field lookup */
        uart_puts("--- field lookup ---");
        fidx = td_field_lookup(desc, "X");
        uart_putstr("  lookup X: idx=");
        print_int(fidx);
        uart_putstr(" offset=");
        print_int(td_field_offset(desc, fidx));
        uart_putstr(" type=");
        uart_puts(nd_type_name(td_field_type(desc, fidx)));

        fidx = td_field_lookup(desc, "Y");
        uart_putstr("  lookup Y: idx=");
        print_int(fidx);
        uart_putstr(" offset=");
        print_int(td_field_offset(desc, fidx));
        uart_putstr(" type=");
        uart_puts(nd_type_name(td_field_type(desc, fidx)));

        fidx = td_field_lookup(desc, "Z");
        uart_putstr("  lookup Z (missing): idx=");
        print_int(fidx);
        uart_putchar(10);

        /* Case insensitive */
        fidx = td_field_lookup(desc, "x");
        uart_putstr("  lookup x (case insens): idx=");
        print_int(fidx);
        uart_putchar(10);
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 9: mixed-type record */
    uart_puts("--- mixed record ---");
    arena_init();
    ast_init();
    parse_init("DCL 1 DEVICE, 3 ID BYTE, 3 NAME(8) CHAR, 3 STATUS WORD;");
    rec = parse_dcl();
    if (!parse_err && rec != NODE_NULL) {
        desc = td_make_record(rec);
        td_print(desc);
    }

    /* Test 10: type error detection */
    uart_puts("--- type errors ---");
    tc_init();
    type_compat_assign(TYPE_RECORD, TYPE_INT24);
    uart_putstr("  record->int err: ");
    uart_puts(tc_err ? "detected" : "missed");

    tc_init();
    type_binop_result(TOK_LT, TYPE_RECORD, TYPE_INT24);
    uart_putstr("  cmp record err: ");
    uart_puts(tc_err ? "detected" : "missed");

    tc_init();
    type_unop_result(TOK_MINUS, TYPE_PTR);
    uart_putstr("  neg PTR err: ");
    uart_puts(tc_err ? "detected" : "missed");

    /* Test 11: argument compatibility */
    uart_puts("--- arg compat ---");
    tc_init();
    uart_putstr("  int->int arg: ");
    uart_puts(type_compat_arg(TYPE_INT24, TYPE_INT24) ? "OK" : "FAIL");
    tc_init();
    uart_putstr("  byte->int arg: ");
    uart_puts(type_compat_arg(TYPE_BYTE, TYPE_INT24) ? "OK" : "FAIL");
    tc_init();
    uart_putstr("  ptr->int arg: ");
    uart_puts(type_compat_arg(TYPE_PTR, TYPE_INT24) ? "OK" : "FAIL");
    tc_init();
    uart_putstr("  rec->int arg: ");
    uart_puts(type_compat_arg(TYPE_RECORD, TYPE_INT24) ? "FAIL" : "OK");
}

void test_layout(void) {
    int prog;
    int proc;
    int fsize;
    int idx;

    /* Test 1: simple scalar DCL widths */
    uart_puts("--- scalar widths ---");
    arena_init();
    ast_init();
    parse_init("DCL X INT(24);");
    int n = parse_dcl();
    uart_putstr("  INT(24) width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    arena_init();
    ast_init();
    parse_init("DCL B BYTE;");
    n = parse_dcl();
    uart_putstr("  BYTE width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    arena_init();
    ast_init();
    parse_init("DCL P PTR;");
    n = parse_dcl();
    uart_putstr("  PTR width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    arena_init();
    ast_init();
    parse_init("DCL C CHAR;");
    n = parse_dcl();
    uart_putstr("  CHAR width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    arena_init();
    ast_init();
    parse_init("DCL W WORD;");
    n = parse_dcl();
    uart_putstr("  WORD width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    /* Test 2: array widths */
    uart_puts("--- array widths ---");
    arena_init();
    ast_init();
    parse_init("DCL BUF(80) CHAR;");
    n = parse_dcl();
    uart_putstr("  CHAR(80) width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    arena_init();
    ast_init();
    parse_init("DCL NUMS(10) INT(24);");
    n = parse_dcl();
    uart_putstr("  INT24(10) width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    arena_init();
    ast_init();
    parse_init("DCL FLAGS(8) BIT;");
    n = parse_dcl();
    uart_putstr("  BIT(8) width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    /* Test 3: record widths */
    uart_puts("--- record widths ---");
    types_init();
    arena_init();
    ast_init();
    parse_init("DCL 1 POINT, 3 X INT(24), 3 Y INT(24);");
    n = parse_dcl();
    uart_putstr("  POINT(2xINT24) width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    arena_init();
    ast_init();
    parse_init("DCL 1 DEV, 3 ID BYTE, 3 NAME(8) CHAR, 3 STATUS WORD;");
    n = parse_dcl();
    uart_putstr("  DEV(BYTE+CHAR8+WORD) width=");
    print_int(layout_dcl_width(n));
    uart_putchar(10);

    /* Test 4: parameter layout */
    uart_puts("--- param layout ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("PROC ADD(A INT(24), B INT(24)) RETURNS(INT(24)); RETURN(A + B); END;");
    proc = parse_proc();
    if (!parse_err && proc != NODE_NULL) {
        fsize = layout_proc(proc);
        uart_putstr("  frame_size=");
        print_int(fsize);
        uart_putchar(10);

        idx = sym_lookup("A");
        uart_putstr("  A: offset=");
        print_int(sym_offset[idx]);
        uart_putstr(" width=");
        print_int(sym_width[idx]);
        uart_putchar(10);

        idx = sym_lookup("B");
        uart_putstr("  B: offset=");
        print_int(sym_offset[idx]);
        uart_putstr(" width=");
        print_int(sym_width[idx]);
        uart_putchar(10);
        sym_exit_scope();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 5: local variable layout */
    uart_puts("--- local layout ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("PROC CALC(X INT(24)); DCL RESULT INT(24); DCL TEMP BYTE; DCL BUF(10) CHAR; RETURN; END;");
    proc = parse_proc();
    if (!parse_err && proc != NODE_NULL) {
        fsize = layout_proc(proc);
        uart_putstr("  frame_size=");
        print_int(fsize);
        uart_putchar(10);

        idx = sym_lookup("X");
        uart_putstr("  X(param): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("RESULT");
        uart_putstr("  RESULT(auto): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("TEMP");
        uart_putstr("  TEMP(auto): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("BUF");
        uart_putstr("  BUF(auto): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        sym_exit_scope();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 6: static local variables */
    uart_puts("--- static locals ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("PROC FOO; DCL COUNTER INT(24) STATIC; DCL LOCAL BYTE; RETURN; END;");
    proc = parse_proc();
    if (!parse_err && proc != NODE_NULL) {
        fsize = layout_proc(proc);
        uart_putstr("  frame_size=");
        print_int(fsize);
        uart_putchar(10);

        idx = sym_lookup("COUNTER");
        uart_putstr("  COUNTER(static): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("LOCAL");
        uart_putstr("  LOCAL(auto): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        layout_print_frame();
        sym_exit_scope();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 7: global declarations */
    uart_puts("--- global layout ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("DCL GCOUNT INT(24); DCL GBUF(80) CHAR; DCL GFLAG BYTE STATIC;");
    prog = parse_program();
    if (!parse_err && prog != NODE_NULL) {
        layout_globals(prog);

        idx = sym_lookup("GCOUNT");
        uart_putstr("  GCOUNT: offset=");
        print_int(sym_offset[idx]);
        uart_putstr(" width=");
        print_int(sym_width[idx]);
        uart_putchar(10);

        idx = sym_lookup("GBUF");
        uart_putstr("  GBUF: offset=");
        print_int(sym_offset[idx]);
        uart_putstr(" width=");
        print_int(sym_width[idx]);
        uart_putchar(10);

        idx = sym_lookup("GFLAG");
        uart_putstr("  GFLAG: offset=");
        print_int(sym_offset[idx]);
        uart_putstr(" width=");
        print_int(sym_width[idx]);
        uart_putchar(10);

        layout_print_frame();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 8: mixed param types (BYTE, PTR, INT24) */
    uart_puts("--- mixed params ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("PROC FILL(DST PTR, VAL BYTE, LEN INT(24)); RETURN; END;");
    proc = parse_proc();
    if (!parse_err && proc != NODE_NULL) {
        fsize = layout_proc(proc);

        idx = sym_lookup("DST");
        uart_putstr("  DST(PTR): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("VAL");
        uart_putstr("  VAL(BYTE): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("LEN");
        uart_putstr("  LEN(INT24): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        sym_exit_scope();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 9: record as local variable */
    uart_puts("--- record local ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("PROC SETUP; DCL 1 PT, 3 X INT(24), 3 Y INT(24); DCL Z INT(24); RETURN; END;");
    proc = parse_proc();
    if (!parse_err && proc != NODE_NULL) {
        fsize = layout_proc(proc);
        uart_putstr("  frame_size=");
        print_int(fsize);
        uart_putchar(10);

        idx = sym_lookup("PT");
        if (idx >= 0) {
            uart_putstr("  PT(record): offset=");
            print_int(sym_offset[idx]);
            uart_putstr(" width=");
            print_int(sym_width[idx]);
            uart_putchar(10);
        }

        idx = sym_lookup("Z");
        uart_putstr("  Z(auto): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        sym_exit_scope();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 10: empty procedure (no params, no locals) */
    uart_puts("--- empty proc ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("PROC NOP; END;");
    proc = parse_proc();
    if (!parse_err && proc != NODE_NULL) {
        fsize = layout_proc(proc);
        uart_putstr("  frame_size=");
        print_int(fsize);
        uart_putchar(10);
        sym_exit_scope();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 11: globals + proc combined */
    uart_puts("--- globals+proc ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("DCL TOTAL INT(24); DCL LIMIT INT(24); PROC INCR; DCL STEP INT(24); TOTAL = TOTAL + STEP; END;");
    prog = parse_program();
    if (!parse_err && prog != NODE_NULL) {
        layout_globals(prog);

        idx = sym_lookup("TOTAL");
        uart_putstr("  TOTAL(global): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("LIMIT");
        uart_putstr("  LIMIT(global): offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        /* Now lay out the proc */
        int child = nd_left[prog];
        while (child != NODE_NULL) {
            if (nd_kind[child] == NODE_PROC) {
                fsize = layout_proc(child);
                uart_putstr("  INCR frame_size=");
                print_int(fsize);
                uart_putchar(10);

                idx = sym_lookup("STEP");
                uart_putstr("  STEP(local): offset=");
                print_int(sym_offset[idx]);
                uart_putchar(10);

                sym_exit_scope();
                break;
            }
            child = nd_next[child];
        }

        layout_print_frame();
    } else {
        uart_puts("  PARSE ERROR");
    }

    /* Test 12: multiple static allocations track correctly */
    uart_puts("--- static tracking ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();

    parse_init("DCL A INT(24); DCL B(10) BYTE; DCL C WORD;");
    prog = parse_program();
    if (!parse_err && prog != NODE_NULL) {
        layout_globals(prog);

        idx = sym_lookup("A");
        uart_putstr("  A: offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("B");
        uart_putstr("  B: offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        idx = sym_lookup("C");
        uart_putstr("  C: offset=");
        print_int(sym_offset[idx]);
        uart_putchar(10);

        uart_putstr("  static_next=");
        print_int(layout_static_next);
        uart_putchar(10);
    } else {
        uart_puts("  PARSE ERROR");
    }
}

void test_emit(void) {
    /* Test 1: basic emit_str and emit_line */
    uart_puts("--- basic emit ---");
    emit_init();
    emit_str("hello");
    emit_nl();
    emit_line("world");
    uart_putstr("  len=");
    print_int(emit_length());
    uart_putchar(10);
    uart_putstr("  buf: ");
    uart_putstr(emit_output());

    /* Test 2: emit_int */
    uart_puts("--- emit_int ---");
    emit_init();
    emit_int(0);
    emit_char(32);
    emit_int(42);
    emit_char(32);
    emit_int(12345);
    emit_char(32);
    emit_int(-7);
    uart_putstr("  ");
    uart_puts(emit_output());

    /* Test 3: label generation */
    uart_puts("--- labels ---");
    emit_init();
    int l0 = emit_new_label();
    int l1 = emit_new_label();
    int l2 = emit_new_label();
    uart_putstr("  l0=");
    print_int(l0);
    uart_putstr(" l1=");
    print_int(l1);
    uart_putstr(" l2=");
    print_int(l2);
    uart_putchar(10);
    emit_label(l0);
    emit_label(l1);
    uart_putstr("  ");
    uart_putstr(emit_output());

    /* Test 4: named labels */
    uart_puts("--- named labels ---");
    emit_init();
    emit_named_label("main");
    emit_named_label("foo");
    uart_putstr(emit_output());

    /* Test 5: instructions */
    uart_puts("--- instructions ---");
    emit_init();
    emit_instr0("nop");
    emit_instr1("push", "fp");
    emit_instr2("mov", "fp", "sp");
    emit_instr("jmp     (r1)");
    uart_putstr(emit_output());

    /* Test 6: comments */
    uart_puts("--- comments ---");
    emit_init();
    emit_comment("PL/SW source line");
    emit_source_line("x = y + 1;");
    uart_putstr(emit_output());

    /* Test 7: section switching */
    uart_puts("--- sections ---");
    emit_init();
    emit_str(EMIT_INDENT);
    emit_line(".text");
    emit_in_data = 0;
    emit_global("main");
    emit_named_label("main");
    emit_prologue();
    emit_epilogue();
    emit_data_section();
    emit_named_label("msg");
    emit_ascii("Hello");
    emit_zero();
    emit_text_section();
    emit_comment("back in text");
    uart_putstr(emit_output());

    /* Test 8: .globl directive */
    uart_puts("--- globl ---");
    emit_init();
    emit_global("myproc");
    uart_putstr(emit_output());

    /* Test 9: data directives */
    uart_puts("--- data directives ---");
    emit_init();
    emit_byte(65);
    emit_word(1000);
    uart_putstr(emit_output());

    /* Test 10: prologue/epilogue match COR24 convention */
    uart_puts("--- prologue/epilogue ---");
    emit_init();
    emit_prologue();
    emit_epilogue();
    uart_putstr(emit_output());

    /* Test 11: generate a complete skeleton .s file */
    uart_puts("--- skeleton .s ---");
    emit_init();
    emit_str(EMIT_INDENT);
    emit_line(".text");
    emit_in_data = 0;
    emit_nl();
    emit_str(EMIT_INDENT);
    emit_line(".globl  _start");
    emit_line("_start:");
    emit_instr2("la", "r0", "_main");
    emit_instr2("jal", "r1", "(r0)");
    emit_line("_halt:");
    emit_instr("bra     _halt");
    emit_nl();
    emit_global("main");
    emit_named_label("main");
    emit_prologue();
    /* load 42 into r0 */
    int ret_label = emit_new_label();
    emit_instr2("lc", "r0", "42");
    emit_label(ret_label);
    emit_epilogue();
    emit_data_section();
    emit_named_label("greeting");
    emit_ascii("Hi");
    emit_zero();
    uart_putstr(emit_output());

    /* Test 12: error flag on buffer full (just check it's 0 normally) */
    uart_puts("--- error check ---");
    uart_putstr("  err=");
    print_int(emit_err);
    uart_putchar(10);
}

void test_codegen(void) {
    /* Helper: reset all state for each test */

    /* Test 1: Literal loading - small constant */
    uart_puts("--- cg: literal small ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int n1 = nd_literal(42);
    cg_expr(n1);
    uart_putstr(emit_output());

    /* Test 2: Literal loading - large constant */
    uart_puts("--- cg: literal large ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int n2 = nd_literal(1000);
    cg_expr(n2);
    uart_putstr(emit_output());

    /* Test 3: Literal loading - negative */
    uart_puts("--- cg: literal neg ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int n3 = nd_literal(-5);
    cg_expr(n3);
    uart_putstr(emit_output());

    /* Test 4: Simple addition (literal + literal) */
    uart_puts("--- cg: add ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int add_n = nd_binop(TOK_PLUS, nd_literal(10), nd_literal(20));
    cg_expr(add_n);
    uart_putstr(emit_output());

    /* Test 5: Subtraction */
    uart_puts("--- cg: sub ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int sub_n = nd_binop(TOK_MINUS, nd_literal(50), nd_literal(8));
    cg_expr(sub_n);
    uart_putstr(emit_output());

    /* Test 6: Multiplication */
    uart_puts("--- cg: mul ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int mul_n = nd_binop(TOK_STAR, nd_literal(7), nd_literal(6));
    cg_expr(mul_n);
    uart_putstr(emit_output());

    /* Test 7: Nested expression: (3 + 4) * 5 */
    uart_puts("--- cg: nested ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int inner = nd_binop(TOK_PLUS, nd_literal(3), nd_literal(4));
    int outer = nd_binop(TOK_STAR, inner, nd_literal(5));
    cg_expr(outer);
    uart_putstr(emit_output());

    /* Test 8: Variable load (automatic) */
    uart_puts("--- cg: var load ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    /* Insert a variable at offset -3 from fp */
    int si = sym_insert("X", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -3;
    int id_n = nd_ident("X");
    cg_expr(id_n);
    uart_putstr(emit_output());

    /* Test 9: Variable load (static) */
    uart_puts("--- cg: var static ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("Y", TYPE_INT24, 3, STOR_STATIC);
    sym_offset[si] = 4096;
    id_n = nd_ident("Y");
    cg_expr(id_n);
    uart_putstr(emit_output());

    /* Test 10: Variable in expression: X + 10 */
    uart_puts("--- cg: var + lit ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("X", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -3;
    int var_add = nd_binop(TOK_PLUS, nd_ident("X"), nd_literal(10));
    cg_expr(var_add);
    uart_putstr(emit_output());

    /* Test 11: Comparison: ==  */
    uart_puts("--- cg: eq ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int eq_n = nd_binop(TOK_EQ, nd_literal(5), nd_literal(5));
    cg_expr(eq_n);
    uart_putstr(emit_output());

    /* Test 12: Comparison: < */
    uart_puts("--- cg: lt ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int lt_n = nd_binop(TOK_LT, nd_literal(3), nd_literal(7));
    cg_expr(lt_n);
    uart_putstr(emit_output());

    /* Test 13: Complex expression with spill: (a+b) * (c+d) */
    uart_puts("--- cg: spill ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int ab = nd_binop(TOK_PLUS, nd_literal(1), nd_literal(2));
    int cd = nd_binop(TOK_PLUS, nd_literal(3), nd_literal(4));
    int abcd = nd_binop(TOK_STAR, ab, cd);
    cg_expr(abcd);
    uart_putstr(emit_output());

    /* Test 14: Unary negate */
    uart_puts("--- cg: negate ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int neg_n = nd_unop(TOK_MINUS, nd_literal(42));
    cg_expr(neg_n);
    uart_putstr(emit_output());

    /* Test 15: Assignment codegen */
    uart_puts("--- cg: assign ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("Z", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -6;
    int asgn = nd_assign(nd_ident("Z"), nd_literal(99));
    cg_assign(asgn);
    uart_putstr(emit_output());

    /* Test 16: BYTE variable (1-byte load/store) */
    uart_puts("--- cg: byte var ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("B", TYPE_BYTE, 1, STOR_AUTO);
    sym_offset[si] = -1;
    int byte_load = nd_ident("B");
    cg_expr(byte_load);
    uart_putstr(emit_output());

    /* Test 17: Division (generates call to __plsw_div) */
    uart_puts("--- cg: div ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    cg_div_emitted = 0;
    int div_n = nd_binop(TOK_SLASH, nd_literal(100), nd_literal(7));
    cg_expr(div_n);
    uart_putstr(emit_output());

    /* Test 18: Comparison operators: >, <=, >= */
    uart_puts("--- cg: gt ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int gt_n = nd_binop(TOK_GT, nd_literal(10), nd_literal(3));
    cg_expr(gt_n);
    uart_putstr(emit_output());

    uart_puts("--- cg: le ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int le_n = nd_binop(TOK_LE, nd_literal(5), nd_literal(10));
    cg_expr(le_n);
    uart_putstr(emit_output());

    uart_puts("--- cg: ne ---");
    arena_init();
    ast_init();
    emit_init();
    cg_init();
    int ne_n = nd_binop(TOK_NE, nd_literal(1), nd_literal(2));
    cg_expr(ne_n);
    uart_putstr(emit_output());

    /* Test 19: error state should be clean */
    uart_puts("--- cg: error check ---");
    uart_putstr("  cg_err=");
    print_int(cg_err);
    uart_putchar(10);
}

void test_assign_codegen(void) {
    int si;
    int asgn;
    int errs = 0;

    /* Test 1: Assign literal to automatic variable */
    uart_puts("--- asgn: auto var ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("X", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -3;
    asgn = nd_assign(nd_ident("X"), nd_literal(42));
    cg_assign(asgn);
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 2: Assign literal to static variable */
    uart_puts("--- asgn: static var ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("S", TYPE_INT24, 3, STOR_STATIC);
    sym_offset[si] = 4096;
    asgn = nd_assign(nd_ident("S"), nd_literal(100));
    cg_assign(asgn);
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 3: Assign expression (a + b) to variable */
    uart_puts("--- asgn: expr ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("R", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -6;
    asgn = nd_assign(nd_ident("R"), nd_binop(TOK_PLUS, nd_literal(10), nd_literal(20)));
    cg_assign(asgn);
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 4: Assign to BYTE variable (1-byte store) */
    uart_puts("--- asgn: byte var ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("B", TYPE_BYTE, 1, STOR_AUTO);
    sym_offset[si] = -1;
    asgn = nd_assign(nd_ident("B"), nd_literal(65));
    cg_assign(asgn);
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 5: Assign then load back (assign X=99, then generate load of X) */
    uart_puts("--- asgn: assign+load ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("V", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -9;
    asgn = nd_assign(nd_ident("V"), nd_literal(99));
    cg_assign(asgn);
    /* Now generate load of V into r0 */
    cg_expr(nd_ident("V"));
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 6: Assign complex expression: X = (3 + 4) * 5 */
    uart_puts("--- asgn: complex expr ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("X", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -3;
    {
        int inner = nd_binop(TOK_PLUS, nd_literal(3), nd_literal(4));
        int outer = nd_binop(TOK_STAR, inner, nd_literal(5));
        asgn = nd_assign(nd_ident("X"), outer);
        cg_assign(asgn);
    }
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 7: Multiple assignments (X=10, Y=20, Z=X+Y pattern) */
    uart_puts("--- asgn: multi ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("X", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -3;
    si = sym_insert("Y", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -6;
    si = sym_insert("Z", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -9;
    /* X = 10 */
    cg_assign(nd_assign(nd_ident("X"), nd_literal(10)));
    /* Y = 20 */
    cg_assign(nd_assign(nd_ident("Y"), nd_literal(20)));
    /* Z = X + Y */
    cg_assign(nd_assign(nd_ident("Z"), nd_binop(TOK_PLUS, nd_ident("X"), nd_ident("Y"))));
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 8: Static byte variable assign */
    uart_puts("--- asgn: static byte ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("SB", TYPE_BYTE, 1, STOR_STATIC);
    sym_offset[si] = 8192;
    asgn = nd_assign(nd_ident("SB"), nd_literal(255));
    cg_assign(asgn);
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 9: Assign with negation: X = -42 */
    uart_puts("--- asgn: neg expr ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("X", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -3;
    asgn = nd_assign(nd_ident("X"), nd_unop(TOK_MINUS, nd_literal(42)));
    cg_assign(asgn);
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Test 10: Large offset variable (outside 8-bit range) */
    uart_puts("--- asgn: large offset ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    si = sym_insert("BIG", TYPE_INT24, 3, STOR_AUTO);
    sym_offset[si] = -300;
    asgn = nd_assign(nd_ident("BIG"), nd_literal(777));
    cg_assign(asgn);
    /* Also load it back to verify large-offset load path */
    cg_expr(nd_ident("BIG"));
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Summary */
    uart_putstr("assign codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_static_data(void) {
    int errs;
    int si;
    int n;
    int prog;
    int idx;
    int init_node;
    int lbl;

    errs = 0;

    /* Test 1: Static variable with no initializer (zero-fill, word) */
    uart_puts("--- static: zero word ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL COUNT INT(24);");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        uart_putstr(emit_output());
    }

    /* Test 2: Static variable with INIT value (word) */
    uart_puts("--- static: init word ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL LIMIT INT(24) INIT(100);");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        uart_putstr(emit_output());
    }

    /* Test 3: Static byte variable with INIT */
    uart_puts("--- static: init byte ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL FLAG BYTE INIT(255);");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        uart_putstr(emit_output());
    }

    /* Test 4: Uninitialized byte (zero-fill) */
    uart_puts("--- static: zero byte ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL STATUS BYTE;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        uart_putstr(emit_output());
    }

    /* Test 5: Multiple static declarations */
    uart_puts("--- static: multiple ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL A INT(24) INIT(10); DCL B INT(24) INIT(20); DCL C BYTE INIT(42);");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        uart_putstr(emit_output());
    }

    /* Test 6: Static array (zero-filled) */
    uart_puts("--- static: zero array ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL BUF(8) BYTE;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        uart_putstr(emit_output());
    }

    /* Test 7: String literal registration and emission */
    uart_puts("--- static: string literal ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    cg_static_init();

    lbl = cg_add_string("Hello");
    if (lbl < 0) {
        uart_puts("  FAIL: cg_add_string failed");
        errs = errs + 1;
    } else {
        uart_putstr("  string label: L");
        print_int(lbl);
        uart_putchar(10);
        /* Emit the load instruction */
        cg_load_string(lbl);
        /* Emit the string table */
        cg_emit_string_table();
        uart_putstr(emit_output());
    }

    /* Test 8: Multiple string literals */
    uart_puts("--- static: multi strings ---");
    arena_init();
    ast_init();
    sym_init();
    emit_init();
    cg_init();
    cg_static_init();

    lbl = cg_add_string("AB");
    cg_load_string(lbl);
    lbl = cg_add_string("CD");
    cg_load_string(lbl);
    cg_emit_string_table();
    uart_putstr(emit_output());

    /* Test 9: Static variable with INIT(0) -- explicit zero */
    uart_puts("--- static: init zero ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL COUNTER INT(24) STATIC INIT(0);");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        uart_putstr(emit_output());
    }

    /* Test 10: Codegen store/load with static var verifies runtime access */
    uart_puts("--- static: codegen access ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    /* Manually set up a static variable and emit assignment + load */
    si = sym_insert("TOTAL", TYPE_INT24, 3, STOR_STATIC);
    sym_offset[si] = 0x1000;
    /* Assign 42 to TOTAL */
    n = nd_assign(nd_ident("TOTAL"), nd_literal(42));
    cg_assign(n);
    /* Load TOTAL back */
    cg_expr(nd_ident("TOTAL"));
    uart_putstr(emit_output());
    if (cg_err) { uart_puts("  FAIL: cg_err set"); errs = errs + 1; }

    /* Summary */
    uart_putstr("static data errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_proc_codegen(void) {
    int errs;
    int prog;
    int child;
    char *out;

    errs = 0;

    /* Test 1: Empty procedure -- prologue/epilogue only */
    uart_puts("--- proc: empty ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC FOO; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Verify key parts present */
        if (!str_find(out, "_FOO:")) {
            uart_puts("  FAIL: missing _FOO label");
            errs = errs + 1;
        }
        if (!str_find(out, "push")) {
            uart_puts("  FAIL: missing push in prologue");
            errs = errs + 1;
        }
        if (!str_find(out, "jmp")) {
            uart_puts("  FAIL: missing jmp in epilogue");
            errs = errs + 1;
        }
    }

    /* Test 2: Procedure with RETURN expression */
    uart_puts("--- proc: return value ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC GETVAL RETURNS(INT(24)); RETURN(42); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_GETVAL:")) {
            uart_puts("  FAIL: missing _GETVAL label");
            errs = errs + 1;
        }
        /* RETURN(42) should load 42 into r0 */
        if (!str_find(out, "42")) {
            uart_puts("  FAIL: missing literal 42");
            errs = errs + 1;
        }
    }

    /* Test 3: Procedure with local variable and assignment */
    uart_puts("--- proc: local var ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC WORK; DCL X INT(24); X = 10; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Should have stack allocation (sub sp) */
        if (!str_find(out, "sub")) {
            uart_puts("  FAIL: missing stack allocation");
            errs = errs + 1;
        }
    }

    /* Test 4: Procedure with parameter */
    uart_puts("--- proc: with param ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC DOUBLE(N INT(24)) RETURNS(INT(24)); RETURN(N + N); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_DOUBLE:")) {
            uart_puts("  FAIL: missing _DOUBLE label");
            errs = errs + 1;
        }
    }

    /* Test 5: NAKED procedure -- no prologue/epilogue */
    uart_puts("--- proc: naked ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC IRQ OPTIONS(NAKED); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_IRQ:")) {
            uart_puts("  FAIL: missing _IRQ label");
            errs = errs + 1;
        }
        /* NAKED should NOT have push fp */
        if (str_find(out, "push")) {
            uart_puts("  FAIL: naked should not have prologue");
            errs = errs + 1;
        }
    }

    /* Test 6: Multiple procedures in one program */
    uart_puts("--- proc: multiple ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC A; END; PROC B; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_A:")) {
            uart_puts("  FAIL: missing _A label");
            errs = errs + 1;
        }
        if (!str_find(out, "_B:")) {
            uart_puts("  FAIL: missing _B label");
            errs = errs + 1;
        }
    }

    /* Test 7: Procedure with multiple locals */
    uart_puts("--- proc: multi locals ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC SWAP; DCL A INT(24); DCL B INT(24); DCL T INT(24); T = A; A = B; B = T; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_SWAP:")) {
            uart_puts("  FAIL: missing _SWAP label");
            errs = errs + 1;
        }
    }

    /* Test 8: FREESTANDING procedure (same codegen as normal) */
    uart_puts("--- proc: freestanding ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN OPTIONS(FREESTANDING); DCL X INT(24); X = 99; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing _MAIN label");
            errs = errs + 1;
        }
    }

    /* Test 9: Procedure with byte local */
    uart_puts("--- proc: byte local ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC FLAGS; DCL F BYTE; F = 1; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Byte store should use sb */
        if (!str_find(out, "sb")) {
            uart_puts("  FAIL: missing sb for byte store");
            errs = errs + 1;
        }
    }

    /* Test 10: Globals + Proc -- static data + code */
    uart_puts("--- proc: globals+proc ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL G INT(24) INIT(7); PROC SET; G = 42; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_G:")) {
            uart_puts("  FAIL: missing _G label");
            errs = errs + 1;
        }
        if (!str_find(out, "_SET:")) {
            uart_puts("  FAIL: missing _SET label");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("proc codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_call_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: CALL with no arguments */
    uart_puts("--- call: no args ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; CALL FOO; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "la      r2,_FOO")) {
            uart_puts("  FAIL: missing la r2,_FOO");
            errs = errs + 1;
        }
        if (!str_find(out, "jal     r1,(r2)")) {
            uart_puts("  FAIL: missing jal");
            errs = errs + 1;
        }
    }

    /* Test 2: CALL with one argument */
    uart_puts("--- call: one arg ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; CALL PRINT(42); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "sub     sp,r2")) {
            uart_puts("  FAIL: missing stack alloc");
            errs = errs + 1;
        }
        if (!str_find(out, "sw      r0,0(sp)")) {
            uart_puts("  FAIL: missing arg store");
            errs = errs + 1;
        }
        if (!str_find(out, "la      r2,_PRINT")) {
            uart_puts("  FAIL: missing call target");
            errs = errs + 1;
        }
        if (!str_find(out, "add     sp,r2")) {
            uart_puts("  FAIL: missing stack cleanup");
            errs = errs + 1;
        }
    }

    /* Test 3: CALL with multiple arguments */
    uart_puts("--- call: multi args ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; CALL ADD3(1, 2, 3); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* 3 args = 9 bytes stack space */
        if (!str_find(out, "lc      r2,9")) {
            uart_puts("  FAIL: missing 9-byte stack alloc");
            errs = errs + 1;
        }
        /* Args stored at sp+0, sp+3, sp+6 */
        if (!str_find(out, "sw      r0,0(sp)")) {
            uart_puts("  FAIL: missing arg1 store at 0(sp)");
            errs = errs + 1;
        }
        if (!str_find(out, "sw      r0,3(sp)")) {
            uart_puts("  FAIL: missing arg2 store at 3(sp)");
            errs = errs + 1;
        }
        if (!str_find(out, "sw      r0,6(sp)")) {
            uart_puts("  FAIL: missing arg3 store at 6(sp)");
            errs = errs + 1;
        }
        if (!str_find(out, "la      r2,_ADD3")) {
            uart_puts("  FAIL: missing call target");
            errs = errs + 1;
        }
    }

    /* Test 4: Call as expression (return value used in assignment) */
    uart_puts("--- call: expr context ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; DCL X INT(24); X = GETVAL(10); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Should call GETVAL then store r0 into X */
        if (!str_find(out, "la      r2,_GETVAL")) {
            uart_puts("  FAIL: missing call to GETVAL");
            errs = errs + 1;
        }
        if (!str_find(out, "jal     r1,(r2)")) {
            uart_puts("  FAIL: missing jal");
            errs = errs + 1;
        }
        /* After call, r0 should be stored to X's stack location */
        if (!str_find(out, "sw      r0,")) {
            uart_puts("  FAIL: missing store of return value");
            errs = errs + 1;
        }
    }

    /* Test 5: Multiple calls in sequence */
    uart_puts("--- call: sequential ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; CALL A; CALL B(1); CALL C(2, 3); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, "_A")) {
            uart_puts("  FAIL: missing call A");
            errs = errs + 1;
        }
        if (!str_find(out, "_B")) {
            uart_puts("  FAIL: missing call B");
            errs = errs + 1;
        }
        if (!str_find(out, "_C")) {
            uart_puts("  FAIL: missing call C");
            errs = errs + 1;
        }
    }

    /* Test 6: Call with variable argument */
    uart_puts("--- call: var arg ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; DCL Y INT(24); CALL SEND(Y); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Should load Y from stack then store as arg */
        if (!str_find(out, "lw      r0,")) {
            uart_puts("  FAIL: missing load of Y");
            errs = errs + 1;
        }
        if (!str_find(out, "la      r2,_SEND")) {
            uart_puts("  FAIL: missing call target");
            errs = errs + 1;
        }
    }

    /* Test 7: Call with expression argument */
    uart_puts("--- call: expr arg ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; CALL PUT(1 + 2); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Should evaluate 1+2 then push result */
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing add for expr arg");
            errs = errs + 1;
        }
        if (!str_find(out, "la      r2,_PUT")) {
            uart_puts("  FAIL: missing call target");
            errs = errs + 1;
        }
    }

    /* Test 8: Nested call -- call result used as argument */
    uart_puts("--- call: nested ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; CALL PRINT(DOUBLE(5)); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Should call DOUBLE first, then PRINT */
        if (!str_find(out, "_DOUBLE")) {
            uart_puts("  FAIL: missing inner call DOUBLE");
            errs = errs + 1;
        }
        if (!str_find(out, "_PRINT")) {
            uart_puts("  FAIL: missing outer call PRINT");
            errs = errs + 1;
        }
    }

    /* Test 9: Call with global data and procedures combined */
    uart_puts("--- call: with globals ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("DCL COUNT INT(24); PROC MAIN; CALL INIT(COUNT); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_emit_static_data(prog);
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        if (!str_find(out, ".data")) {
            uart_puts("  FAIL: missing .data section");
            errs = errs + 1;
        }
        if (!str_find(out, "_INIT")) {
            uart_puts("  FAIL: missing call to INIT");
            errs = errs + 1;
        }
    }

    /* Test 10: Call in expression with arithmetic */
    uart_puts("--- call: in expr ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; DCL R INT(24); R = CALC(1) + 10; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);
        /* Call CALC, then add 10, then store in R */
        if (!str_find(out, "_CALC")) {
            uart_puts("  FAIL: missing call to CALC");
            errs = errs + 1;
        }
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing add for + 10");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("call codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

/* ===== Recursion Codegen Tests ===== */

void test_recursion_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: Recursive factorial -- frame management and self-call.
     * FACT(N) calls FACT(N-1), so each invocation must get its own
     * frame with saved fp, r1 (return addr), r2. */
    uart_puts("--- recursion: factorial ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC FACT(N INT(24)) RETURNS(INT(24)); IF N <= 1 THEN RETURN(1); RETURN(N * FACT(N - 1)); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have prologue (saves frame for each call) */
        if (!str_find(out, "push     fp")) {
            uart_puts("  FAIL: missing push fp (prologue)");
            errs = errs + 1;
        }
        if (!str_find(out, "push     r1")) {
            uart_puts("  FAIL: missing push r1 (saves return addr)");
            errs = errs + 1;
        }
        if (!str_find(out, "mov     fp,sp")) {
            uart_puts("  FAIL: missing mov fp,sp (frame setup)");
            errs = errs + 1;
        }

        /* Must have self-call to FACT */
        if (!str_find(out, "_FACT")) {
            uart_puts("  FAIL: missing self-call to FACT");
            errs = errs + 1;
        }
        if (!str_find(out, "jal     r1,(r2)")) {
            uart_puts("  FAIL: missing jal (call instruction)");
            errs = errs + 1;
        }

        /* Must have epilogue (restores frame on return) */
        if (!str_find(out, "mov     sp,fp")) {
            uart_puts("  FAIL: missing mov sp,fp (epilogue)");
            errs = errs + 1;
        }
        if (!str_find(out, "pop     r1")) {
            uart_puts("  FAIL: missing pop r1 (restore return addr)");
            errs = errs + 1;
        }
        if (!str_find(out, "pop     fp")) {
            uart_puts("  FAIL: missing pop fp (restore caller frame)");
            errs = errs + 1;
        }
        if (!str_find(out, "jmp     (r1)")) {
            uart_puts("  FAIL: missing jmp (r1) (return)");
            errs = errs + 1;
        }

        /* IF condition: N <= 1 comparison */
        if (!str_find(out, "ceq     r0,z")) {
            uart_puts("  FAIL: missing condition branch");
            errs = errs + 1;
        }

        /* Argument push for recursive call (N-1) */
        if (!str_find(out, "sub     sp,r2")) {
            uart_puts("  FAIL: missing arg space allocation");
            errs = errs + 1;
        }
        /* Cleanup after call */
        if (!str_find(out, "add     sp,r2")) {
            uart_puts("  FAIL: missing arg cleanup");
            errs = errs + 1;
        }

        /* Multiply N * FACT(N-1): must push/pop to save N across call */
        if (!str_find(out, "mul     r0,r1")) {
            uart_puts("  FAIL: missing multiply for N * FACT(N-1)");
            errs = errs + 1;
        }
    }

    /* Test 2: Nested calls -- PROC calls another with result of a third.
     * ADD(MUL(2, 3), MUL(4, 5)) -- each nested call must preserve caller state. */
    uart_puts("--- recursion: nested calls ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MAIN; DCL R INT(24); R = ADD(MUL(2, 3), MUL(4, 5)); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have calls to both MUL and ADD */
        if (!str_find(out, "_MUL")) {
            uart_puts("  FAIL: missing call to MUL");
            errs = errs + 1;
        }
        if (!str_find(out, "_ADD")) {
            uart_puts("  FAIL: missing call to ADD");
            errs = errs + 1;
        }

        /* First MUL result stored at arg slot 0, second at slot 3 */
        if (!str_find(out, "sw      r0,0(sp)")) {
            uart_puts("  FAIL: missing store of first nested result");
            errs = errs + 1;
        }
        if (!str_find(out, "sw      r0,3(sp)")) {
            uart_puts("  FAIL: missing store of second nested result");
            errs = errs + 1;
        }
    }

    /* Test 3: Sequential recursive calls -- verify multiple self-references */
    uart_puts("--- recursion: sequential self-calls ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC FIB(N INT(24)) RETURNS(INT(24)); IF N <= 1 THEN RETURN(N); RETURN(FIB(N - 1) + FIB(N - 2)); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have self-call to FIB */
        if (!str_find(out, "_FIB")) {
            uart_puts("  FAIL: missing self-call to FIB");
            errs = errs + 1;
        }

        /* Two calls mean two jal instructions */
        /* FIB(N-1) + FIB(N-2): first result pushed, second evaluated,
         * then pop and add */
        if (!str_find(out, "push     r0")) {
            uart_puts("  FAIL: missing push r0 for saving FIB(N-1) result");
            errs = errs + 1;
        }
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing add for FIB(N-1) + FIB(N-2)");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("recursion codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_if_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: Simple IF/THEN (no else) */
    uart_puts("--- if: simple if/then ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC CHECK(X INT(24)); IF X > 0 THEN X = X + 1; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Condition evaluation: compare must exist */
        if (!str_find(out, "ceq     r0,z")) {
            uart_puts("  FAIL: missing condition branch (ceq r0,z)");
            errs = errs + 1;
        }
        /* Branch to skip then-body when false */
        if (!str_find(out, "bc      ")) {
            uart_puts("  FAIL: missing bc (conditional branch)");
            errs = errs + 1;
        }
        /* End label must exist */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing end label (L0)");
            errs = errs + 1;
        }
        /* No jmp to label (no else means no jump-over-else) */
        if (str_find(out, "jmp     L")) {
            uart_puts("  FAIL: unexpected jmp in if/then (no else)");
            errs = errs + 1;
        }
    }

    /* Test 2: IF/THEN/ELSE */
    uart_puts("--- if: if/then/else ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC ABS(X INT(24)) RETURNS(INT(24)); IF X < 0 THEN RETURN(0 - X); ELSE RETURN(X); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Condition branch */
        if (!str_find(out, "ceq     r0,z")) {
            uart_puts("  FAIL: missing condition branch (ceq r0,z)");
            errs = errs + 1;
        }
        if (!str_find(out, "bc      ")) {
            uart_puts("  FAIL: missing bc (conditional branch to else)");
            errs = errs + 1;
        }
        /* Must have jmp to skip else block after then */
        if (!str_find(out, "jmp     L")) {
            uart_puts("  FAIL: missing jmp (skip else after then)");
            errs = errs + 1;
        }
        /* Must have at least 2 labels (else and end) */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing else label (L0)");
            errs = errs + 1;
        }
        if (!str_find(out, "L1:")) {
            uart_puts("  FAIL: missing end label (L1)");
            errs = errs + 1;
        }
    }

    /* Test 3: IF/THEN/ELSE with DO blocks */
    uart_puts("--- if: block if/then/else ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC CLAMP(X INT(24), LO INT(24), HI INT(24)) RETURNS(INT(24)); IF X < LO THEN DO; X = LO; RETURN(X); END; ELSE DO; IF X > HI THEN X = HI; RETURN(X); END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Nested IF inside else must produce additional labels */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing label L0");
            errs = errs + 1;
        }
        /* At least 3 labels for outer if/else + inner if */
        if (!str_find(out, "L2:")) {
            uart_puts("  FAIL: missing label L2 (nested if needs labels)");
            errs = errs + 1;
        }
        /* Multiple ceq for outer and inner conditions */
        /* Just ensure codegen completes without error */
        if (!str_find(out, "_CLAMP:")) {
            uart_puts("  FAIL: missing proc label _CLAMP");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("if codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_do_while_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: Simple DO WHILE loop */
    uart_puts("--- do_while: simple loop ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC COUNTDOWN(N INT(24)); DO WHILE (N > 0); N = N - 1; END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Loop header label must exist */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing loop header label (L0)");
            errs = errs + 1;
        }
        /* Condition evaluation: compare for branch */
        if (!str_find(out, "ceq     r0,z")) {
            uart_puts("  FAIL: missing condition test (ceq r0,z)");
            errs = errs + 1;
        }
        /* Conditional branch to end */
        if (!str_find(out, "bc      ")) {
            uart_puts("  FAIL: missing bc (branch to end)");
            errs = errs + 1;
        }
        /* Unconditional branch back to top */
        if (!str_find(out, "jmp     L0")) {
            uart_puts("  FAIL: missing jmp back to header (jmp L0)");
            errs = errs + 1;
        }
        /* End label */
        if (!str_find(out, "L1:")) {
            uart_puts("  FAIL: missing loop end label (L1)");
            errs = errs + 1;
        }
    }

    /* Test 2: DO WHILE with procedure call (counted loop printing) */
    uart_puts("--- do_while: loop with call ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC PRINT_INT(V INT(24)); END; PROC LOOP10(); DCL I INT(24); I = 1; DO WHILE (I <= 10); CALL PRINT_INT(I); I = I + 1; END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have LOOP10 proc label */
        if (!str_find(out, "_LOOP10:")) {
            uart_puts("  FAIL: missing proc label _LOOP10");
            errs = errs + 1;
        }
        /* Must have call to PRINT_INT */
        if (!str_find(out, "la      r2,_PRINT_INT")) {
            uart_puts("  FAIL: missing call to _PRINT_INT");
            errs = errs + 1;
        }
        /* Loop structure: header, condition, body, jump back */
        if (!str_find(out, "jmp     L")) {
            uart_puts("  FAIL: missing jump back to loop header");
            errs = errs + 1;
        }
    }

    /* Test 3: Nested DO WHILE loops */
    uart_puts("--- do_while: nested loops ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC NESTED(N INT(24)); DCL I INT(24); DCL J INT(24); I = 0; DO WHILE (I < N); J = 0; DO WHILE (J < N); J = J + 1; END; I = I + 1; END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Two loop headers (L0 and L2) and two end labels (L1 and L3) */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing outer loop header (L0)");
            errs = errs + 1;
        }
        if (!str_find(out, "L2:")) {
            uart_puts("  FAIL: missing inner loop header (L2)");
            errs = errs + 1;
        }
        /* Jump back for both loops */
        if (!str_find(out, "jmp     L0")) {
            uart_puts("  FAIL: missing outer loop jmp back");
            errs = errs + 1;
        }
        if (!str_find(out, "jmp     L2")) {
            uart_puts("  FAIL: missing inner loop jmp back");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("do_while codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_do_count_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: Simple DO count loop */
    uart_puts("--- do_count: simple loop ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC PRINT_INT(V INT(24)); END; PROC TEST(); DCL I INT(24); DO I = 1 TO 10; CALL PRINT_INT(I); END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have TEST proc label */
        if (!str_find(out, "_TEST:")) {
            uart_puts("  FAIL: missing proc label _TEST");
            errs = errs + 1;
        }
        /* Loop header label */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing loop header label (L0)");
            errs = errs + 1;
        }
        /* Comparison: cls for I > end check */
        if (!str_find(out, "cls     r1,r0")) {
            uart_puts("  FAIL: missing cls comparison (cls r1,r0)");
            errs = errs + 1;
        }
        /* Branch to end when I > end */
        if (!str_find(out, "bc      ")) {
            uart_puts("  FAIL: missing bc (branch to end)");
            errs = errs + 1;
        }
        /* Increment: add r0,r1 */
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing increment (add r0,r1)");
            errs = errs + 1;
        }
        /* Jump back to header */
        if (!str_find(out, "jmp     L0")) {
            uart_puts("  FAIL: missing jmp back to header (jmp L0)");
            errs = errs + 1;
        }
        /* End label */
        if (!str_find(out, "L1:")) {
            uart_puts("  FAIL: missing loop end label (L1)");
            errs = errs + 1;
        }
        /* Must have call to PRINT_INT */
        if (!str_find(out, "la      r2,_PRINT_INT")) {
            uart_puts("  FAIL: missing call to _PRINT_INT");
            errs = errs + 1;
        }
    }

    /* Test 2: DO count with expression bounds */
    uart_puts("--- do_count: expression bounds ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST2(N INT(24)); DCL J INT(24); DO J = 0 TO N; J = J; END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Loop structure must exist */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing loop header label (L0)");
            errs = errs + 1;
        }
        /* End comparison uses cls */
        if (!str_find(out, "cls     r1,r0")) {
            uart_puts("  FAIL: missing cls comparison");
            errs = errs + 1;
        }
        /* End label */
        if (!str_find(out, "L1:")) {
            uart_puts("  FAIL: missing loop end label (L1)");
            errs = errs + 1;
        }
    }

    /* Test 3: Nested DO count and DO WHILE */
    uart_puts("--- do_count: nested with do_while ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST3(); DCL I INT(24); DCL J INT(24); DO I = 1 TO 5; DO WHILE (J > 0); J = J - 1; END; END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Outer DO count header (L0) */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing outer loop header (L0)");
            errs = errs + 1;
        }
        /* Inner DO WHILE header (L2) */
        if (!str_find(out, "L2:")) {
            uart_puts("  FAIL: missing inner loop header (L2)");
            errs = errs + 1;
        }
        /* Jump back for both loops */
        if (!str_find(out, "jmp     L0")) {
            uart_puts("  FAIL: missing outer loop jmp back");
            errs = errs + 1;
        }
        if (!str_find(out, "jmp     L2")) {
            uart_puts("  FAIL: missing inner loop jmp back");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("do_count codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

int main() {
    uart_puts("PL/SW Compiler v0.1");
    uart_puts("COR24 target");
    uart_puts("");

    uart_puts("=== String Tests ===");
    test_strings();
    uart_puts("");

    uart_puts("=== Arena Tests ===");
    test_arena();
    uart_puts("");

    uart_puts("=== Token Tests ===");
    test_tokens();
    uart_puts("");

    uart_puts("=== Lexer Tests ===");
    test_lexer();
    uart_puts("");

    uart_puts("=== AST Tests ===");
    test_ast();
    uart_puts("");

    uart_puts("=== DCL Parser Tests ===");
    test_parser();
    uart_puts("");

    uart_puts("=== Expression Parser Tests ===");
    test_expr_parser();
    uart_puts("");

    uart_puts("=== Statement Parser Tests ===");
    test_stmt_parser();
    uart_puts("");

    uart_puts("=== Procedure Parser Tests ===");
    test_proc_parser();
    uart_puts("");

    uart_puts("=== Top-Level Parser Tests ===");
    test_toplevel_parser();
    uart_puts("");

    uart_puts("=== Symbol Table Tests ===");
    test_symtab();
    uart_puts("");

    uart_puts("=== Type System Tests ===");
    test_types();
    uart_puts("");

    uart_puts("=== Storage Layout Tests ===");
    test_layout();
    uart_puts("");

    uart_puts("=== Emitter Framework Tests ===");
    test_emit();
    uart_puts("");

    uart_puts("=== Expression Codegen Tests ===");
    test_codegen();
    uart_puts("");

    uart_puts("=== Assignment Codegen Tests ===");
    test_assign_codegen();
    uart_puts("");

    uart_puts("=== Static Data Tests ===");
    test_static_data();
    uart_puts("");

    uart_puts("=== Procedure Codegen Tests ===");
    test_proc_codegen();
    uart_puts("");

    uart_puts("=== Call Codegen Tests ===");
    test_call_codegen();
    uart_puts("");

    uart_puts("=== Recursion Codegen Tests ===");
    test_recursion_codegen();
    uart_puts("");

    uart_puts("=== IF Codegen Tests ===");
    test_if_codegen();
    uart_puts("");

    uart_puts("=== DO WHILE Codegen Tests ===");
    test_do_while_codegen();
    uart_puts("");

    uart_puts("=== DO COUNT Codegen Tests ===");
    test_do_count_codegen();
    uart_puts("");

    uart_puts("=== REPL (tokenizer) ===");
    char line[LINE_MAX];

    while (1) {
        uart_putstr("> ");
        int len = uart_getline(line, LINE_MAX);
        if (len == 0) continue;
        lex_init(line, str_len(line));
        while (1) {
            lex_scan();
            tok_print();
            if (cur_type == TOK_EOF) break;
        }
    }

    return 0;
}
