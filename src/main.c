#include "io.h"
#include "arena.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "symtab.h"

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
