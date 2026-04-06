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
#include "macro.h"

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
        /* Should have stack allocation (add sp,-N) */
        if (!str_find(out, "add     sp,-")) {
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
        if (!str_find(out, "push    r0")) {
            uart_puts("  FAIL: missing push r0 for arg");
            errs = errs + 1;
        }
        if (!str_find(out, "la      r2,_PRINT")) {
            uart_puts("  FAIL: missing call target");
            errs = errs + 1;
        }
        if (!str_find(out, "add     sp,3")) {
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
        /* 3 args pushed R-to-L, then cleaned up with add sp,9 */
        if (!str_find(out, "push    r0")) {
            uart_puts("  FAIL: missing push for args");
            errs = errs + 1;
        }
        if (!str_find(out, "la      r2,_ADD3")) {
            uart_puts("  FAIL: missing call target");
            errs = errs + 1;
        }
        if (!str_find(out, "add     sp,9")) {
            uart_puts("  FAIL: missing 9-byte stack cleanup");
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
        if (!str_find(out, "push    r0")) {
            uart_puts("  FAIL: missing push for arg");
            errs = errs + 1;
        }
        /* Cleanup after call */
        if (!str_find(out, "add     sp,3")) {
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

        /* Nested call results pushed onto stack for ADD */
        if (!str_find(out, "push    r0")) {
            uart_puts("  FAIL: missing push of nested result");
            errs = errs + 1;
        }
        if (!str_find(out, "add     sp,6")) {
            uart_puts("  FAIL: missing 6-byte cleanup for ADD(2 args)");
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
        if (!str_find(out, "brt     ")) {
            uart_puts("  FAIL: missing bc (conditional branch)");
            errs = errs + 1;
        }
        /* End label must exist */
        if (!str_find(out, "L0:")) {
            uart_puts("  FAIL: missing end label (L0)");
            errs = errs + 1;
        }
        /* No jmp to label (no else means no jump-over-else) */
        if (str_find(out, "bra     L")) {
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
        if (!str_find(out, "brt     ")) {
            uart_puts("  FAIL: missing bc (conditional branch to else)");
            errs = errs + 1;
        }
        /* Must have jmp to skip else block after then */
        if (!str_find(out, "bra     L")) {
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
        if (!str_find(out, "brt     ")) {
            uart_puts("  FAIL: missing bc (branch to end)");
            errs = errs + 1;
        }
        /* Unconditional branch back to top */
        if (!str_find(out, "bra     L0")) {
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
        if (!str_find(out, "bra     L")) {
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
        if (!str_find(out, "bra     L0")) {
            uart_puts("  FAIL: missing outer loop jmp back");
            errs = errs + 1;
        }
        if (!str_find(out, "bra     L2")) {
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
        if (!str_find(out, "brt     ")) {
            uart_puts("  FAIL: missing bc (branch to end)");
            errs = errs + 1;
        }
        /* Increment: add r0,r1 */
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing increment (add r0,r1)");
            errs = errs + 1;
        }
        /* Jump back to header */
        if (!str_find(out, "bra     L0")) {
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
        if (!str_find(out, "bra     L0")) {
            uart_puts("  FAIL: missing outer loop jmp back");
            errs = errs + 1;
        }
        if (!str_find(out, "bra     L2")) {
            uart_puts("  FAIL: missing inner loop jmp back");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("do_count codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_record_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: Simple record field assignment and read */
    uart_puts("--- record: field assign and read ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST(); DCL 1 REC, 3 X INT(24), 3 Y INT(24), 3 Z BYTE; REC.X = 42; REC.Y = REC.X + 1; REC.Z = 7; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have proc label */
        if (!str_find(out, "_TEST:")) {
            uart_puts("  FAIL: missing proc label _TEST");
            errs = errs + 1;
        }
        /* REC.X assignment: compute addr (fp + rec_off + 0), store */
        if (!str_find(out, "sw      r0,0(r2)")) {
            uart_puts("  FAIL: missing sw r0,0(r2) for field store");
            errs = errs + 1;
        }
        /* REC.Z assignment: byte store since Z is BYTE */
        if (!str_find(out, "sb      r0,0(r2)")) {
            uart_puts("  FAIL: missing sb r0,0(r2) for byte field store");
            errs = errs + 1;
        }
        /* Field load (REC.X in expression): lw r0,0(r2) */
        if (!str_find(out, "lw      r0,0(r2)")) {
            uart_puts("  FAIL: missing lw r0,0(r2) for field load");
            errs = errs + 1;
        }
        /* Must have add for fp-relative address computation */
        if (!str_find(out, "add     r2,fp")) {
            uart_puts("  FAIL: missing add r2,fp for address computation");
            errs = errs + 1;
        }
    }

    /* Test 2: Record with different field types and offsets */
    uart_puts("--- record: field offset verification ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST2(); DCL 1 PT, 3 A BYTE, 3 B INT(24), 3 C BYTE; PT.A = 1; PT.B = 2; PT.C = 3; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have proc label */
        if (!str_find(out, "_TEST2:")) {
            uart_puts("  FAIL: missing proc label _TEST2");
            errs = errs + 1;
        }
        /* PT.A: byte store at record_off + 0 */
        if (!str_find(out, "sb      r0,0(r2)")) {
            uart_puts("  FAIL: missing sb for PT.A byte store");
            errs = errs + 1;
        }
        /* PT.B: word store at record_off + 1 */
        if (!str_find(out, "sw      r0,0(r2)")) {
            uart_puts("  FAIL: missing sw for PT.B word store");
            errs = errs + 1;
        }
    }

    /* Test 3: Record field in expression (read back) */
    uart_puts("--- record: field in expression ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC PRINT_INT(V INT(24)); END; PROC TEST3(); DCL 1 REC, 3 X INT(24), 3 Y INT(24); REC.X = 10; REC.Y = 20; CALL PRINT_INT(REC.X + REC.Y); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have call to PRINT_INT */
        if (!str_find(out, "la      r2,_PRINT_INT")) {
            uart_puts("  FAIL: missing call to _PRINT_INT");
            errs = errs + 1;
        }
        /* Must load field values (lw from computed address) */
        if (!str_find(out, "lw      r0,0(r2)")) {
            uart_puts("  FAIL: missing lw r0,0(r2) for field load in expr");
            errs = errs + 1;
        }
        /* Addition of field values */
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing add r0,r1 for field sum");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("record codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

/* ========== Array Codegen Tests ========== */

void test_array_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: INT array store and load */
    uart_puts("--- array: INT array store/load ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST(); DCL ARR(5) INT(24); ARR(0) = 42; ARR(2) = ARR(0) + 1; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have proc label */
        if (!str_find(out, "_TEST:")) {
            uart_puts("  FAIL: missing proc label _TEST");
            errs = errs + 1;
        }
        /* Array store: sw r0,0(r2) */
        if (!str_find(out, "sw      r0,0(r2)")) {
            uart_puts("  FAIL: missing sw r0,0(r2) for array store");
            errs = errs + 1;
        }
        /* Array load: lw r0,0(r2) */
        if (!str_find(out, "lw      r0,0(r2)")) {
            uart_puts("  FAIL: missing lw r0,0(r2) for array load");
            errs = errs + 1;
        }
        /* Index * 3 for INT: mov r1,r0 + add r0,r1 sequence */
        if (!str_find(out, "mov     r1,r0")) {
            uart_puts("  FAIL: missing mov r1,r0 for index*3 multiply");
            errs = errs + 1;
        }
    }

    /* Test 2: CHAR array (1-byte elements) */
    uart_puts("--- array: CHAR array byte access ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST2(); DCL BUF(10) CHAR; BUF(0) = 65; BUF(1) = BUF(0); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Byte store: sb r0,0(r2) */
        if (!str_find(out, "sb      r0,0(r2)")) {
            uart_puts("  FAIL: missing sb r0,0(r2) for CHAR array store");
            errs = errs + 1;
        }
        /* Byte load: lb r0,0(r2) */
        if (!str_find(out, "lbu     r0,0(r2)")) {
            uart_puts("  FAIL: missing lb r0,0(r2) for CHAR array load");
            errs = errs + 1;
        }
    }

    /* Test 3: Array in loop -- fill and read back */
    uart_puts("--- array: fill in DO loop ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC PRINT_INT(V INT(24)); END; PROC TEST3(); DCL ARR(5) INT(24); DCL I INT(24); DO I = 0 TO 4; ARR(I) = I + 1; END; CALL PRINT_INT(ARR(2)); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must have DO count loop structure (loop label + branch) */
        if (!str_find(out, "_TEST3:")) {
            uart_puts("  FAIL: missing proc label _TEST3");
            errs = errs + 1;
        }
        /* Array store inside loop */
        if (!str_find(out, "sw      r0,0(r2)")) {
            uart_puts("  FAIL: missing sw for array store in loop");
            errs = errs + 1;
        }
        /* Call to PRINT_INT with array element */
        if (!str_find(out, "la      r2,_PRINT_INT")) {
            uart_puts("  FAIL: missing call to _PRINT_INT");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("array codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

/* ========== Pointer Codegen Tests ========== */

void test_pointer_codegen(void) {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: ADDR of a local variable */
    uart_puts("--- pointer: ADDR of local var ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST(); DCL X INT(24); DCL P PTR; X = 42; P = ADDR(X); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_TEST:")) {
            uart_puts("  FAIL: missing proc label _TEST");
            errs = errs + 1;
        }
        /* ADDR computes fp + offset: add r0,fp */
        if (!str_find(out, "add     r0,fp")) {
            uart_puts("  FAIL: missing add r0,fp for ADDR computation");
            errs = errs + 1;
        }
    }

    /* Test 2: ADDR of a record, then ptr->field access */
    uart_puts("--- pointer: ADDR(record) and ptr->field ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST2(); DCL 1 REC, 3 X INT(24), 3 Y INT(24); DCL P PTR; REC.X = 10; REC.Y = 20; P = ADDR(REC); P->X = 99; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_TEST2:")) {
            uart_puts("  FAIL: missing proc label _TEST2");
            errs = errs + 1;
        }
        /* ptr->field store: sw r0,0(r2) */
        if (!str_find(out, "sw      r0,0(r2)")) {
            uart_puts("  FAIL: missing sw r0,0(r2) for ptr->field store");
            errs = errs + 1;
        }
        /* Pointer load: lw for loading P */
        if (!str_find(out, "add     r0,fp")) {
            uart_puts("  FAIL: missing add r0,fp for ADDR");
            errs = errs + 1;
        }
    }

    /* Test 3: Read via ptr->field */
    uart_puts("--- pointer: read via ptr->field ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC PRINT_INT(V INT(24)); END; PROC TEST3(); DCL 1 REC, 3 A INT(24), 3 B INT(24); DCL P PTR; REC.A = 100; REC.B = 200; P = ADDR(REC); CALL PRINT_INT(P->A + P->B); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Must call PRINT_INT */
        if (!str_find(out, "la      r2,_PRINT_INT")) {
            uart_puts("  FAIL: missing call to _PRINT_INT");
            errs = errs + 1;
        }
        /* ptr->field load uses lw r0,0(r2) */
        if (!str_find(out, "lw      r0,0(r2)")) {
            uart_puts("  FAIL: missing lw r0,0(r2) for ptr->field load");
            errs = errs + 1;
        }
        /* Addition of dereferenced values */
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing add r0,r1 for ptr field sum");
            errs = errs + 1;
        }
    }

    /* Test 4: Pointer arithmetic */
    uart_puts("--- pointer: pointer arithmetic ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC TEST4(); DCL X INT(24); DCL P PTR; P = ADDR(X); P = P + 3; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        /* Pointer arithmetic: add instruction */
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing add r0,r1 for pointer arithmetic");
            errs = errs + 1;
        }
        /* ADDR: add r0,fp */
        if (!str_find(out, "add     r0,fp")) {
            uart_puts("  FAIL: missing add r0,fp for ADDR(X)");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("pointer codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

/* ---- Inline ASM Codegen Tests ---- */
void test_asm_codegen() {
    int errs;
    int prog;
    char *out;

    errs = 0;

    /* Test 1: ASM block that writes to LED MMIO address */
    uart_puts("--- asm: LED MMIO write ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC LED_ON(); ASM DO; 'la      r0,0xFF0000'; 'lc      r1,1'; 'sw      r1,0(r0)'; END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_LED_ON:")) {
            uart_puts("  FAIL: missing proc label _LED_ON");
            errs = errs + 1;
        }
        if (!str_find(out, "la      r0,0xFF0000")) {
            uart_puts("  FAIL: missing la r0,0xFF0000");
            errs = errs + 1;
        }
        if (!str_find(out, "lc      r1,1")) {
            uart_puts("  FAIL: missing lc r1,1");
            errs = errs + 1;
        }
        if (!str_find(out, "sw      r1,0(r0)")) {
            uart_puts("  FAIL: missing sw r1,0(r0)");
            errs = errs + 1;
        }
    }

    /* Test 2: NAKED procedure with ASM body (interrupt handler stub) */
    uart_puts("--- asm: NAKED interrupt handler ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC ISR OPTIONS(NAKED); ASM DO; 'push    r0'; 'push    r1'; 'pop     r1'; 'pop     r0'; 'jmp     (iv)'; END; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_ISR:")) {
            uart_puts("  FAIL: missing proc label _ISR");
            errs = errs + 1;
        }
        /* NAKED: should NOT have prologue push fp */
        if (str_find(out, "push     fp")) {
            uart_puts("  FAIL: NAKED proc should not have prologue");
            errs = errs + 1;
        }
        if (!str_find(out, "push    r0")) {
            uart_puts("  FAIL: missing push r0 in ISR");
            errs = errs + 1;
        }
        if (!str_find(out, "jmp     (iv)")) {
            uart_puts("  FAIL: missing jmp (iv) in ISR");
            errs = errs + 1;
        }
    }

    /* Test 3: ASM block inside a regular procedure */
    uart_puts("--- asm: ASM block in regular proc ---");
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("PROC MIXED(); DCL X INT(24); X = 42; ASM DO; 'nop'; 'nop'; END; X = 99; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_MIXED:")) {
            uart_puts("  FAIL: missing proc label _MIXED");
            errs = errs + 1;
        }
        /* Should have prologue (not NAKED) */
        if (!str_find(out, "push     fp")) {
            uart_puts("  FAIL: regular proc should have prologue");
            errs = errs + 1;
        }
        /* Should have nop from ASM block */
        if (!str_find(out, "nop")) {
            uart_puts("  FAIL: missing nop from ASM block");
            errs = errs + 1;
        }
    }

    /* Summary */
    uart_putstr("inline asm codegen errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_include(void) {
    int errs;
    int prog;
    char *out;
    int idx;

    errs = 0;

    /* Test 1: basic %INCLUDE inserts declarations */
    uart_puts("--- include: basic ---");
    inc_init();
    inc_register("DEFS", "DCL COUNT INT(24); DCL FLAG BYTE;");

    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("%INCLUDE DEFS; PROC MAIN(); COUNT = 42; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing proc label _MAIN");
            errs = errs + 1;
        }
        /* Verify no undefined variable error for COUNT */
        if (str_find(out, "undefined variable")) {
            uart_puts("  FAIL: COUNT not resolved from include");
            errs = errs + 1;
        }
    }

    /* Test 2: %INCLUDE with .msw extension lookup */
    uart_puts("--- include: .msw extension ---");
    inc_init();
    inc_register("TYPES.msw", "DCL BUFSIZE INT(24);");

    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("%INCLUDE TYPES; PROC TEST(); BUFSIZE = 100; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_TEST:")) {
            uart_puts("  FAIL: missing proc label _TEST");
            errs = errs + 1;
        }
        if (str_find(out, "undefined variable")) {
            uart_puts("  FAIL: BUFSIZE not resolved from .msw include");
            errs = errs + 1;
        }
    }

    /* Test 3: nested includes */
    uart_puts("--- include: nested ---");
    inc_init();
    inc_register("INNER", "DCL DEEP INT(24);");
    inc_register("OUTER", "%INCLUDE INNER; DCL SHALLOW BYTE;");

    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("%INCLUDE OUTER; PROC NEST(); DEEP = 1; SHALLOW = 2; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        layout_globals(prog);
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_NEST:")) {
            uart_puts("  FAIL: missing proc label _NEST");
            errs = errs + 1;
        }
        if (str_find(out, "undefined variable")) {
            uart_puts("  FAIL: nested include vars not resolved");
            errs = errs + 1;
        }
    }

    /* Test 4: %INCLUDE with procedure definitions */
    uart_puts("--- include: proc def ---");
    inc_init();
    inc_register("HELPER", "PROC ADD(A, B) RETURNS(INT(24)); DCL A INT(24); DCL B INT(24); RETURN(A + B); END;");

    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();

    parse_init("%INCLUDE HELPER; PROC MAIN(); CALL ADD(10, 20); END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        cg_program_procs(prog);
        out = emit_output();
        uart_putstr(out);

        if (!str_find(out, "_ADD:")) {
            uart_puts("  FAIL: missing proc label _ADD from include");
            errs = errs + 1;
        }
        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing proc label _MAIN");
            errs = errs + 1;
        }
    }

    /* Test 5: % not followed by INCLUDE still works as TOK_PERCENT */
    uart_puts("--- include: percent not include ---");
    inc_init();
    arena_init();
    ast_init();

    lex_init("%FOO", 4);
    lex_scan();
    if (cur_type != TOK_PERCENT) {
        uart_puts("  FAIL: expected TOK_PERCENT for %FOO");
        errs = errs + 1;
    } else {
        uart_puts("  OK: %FOO -> TOK_PERCENT");
    }

    /* Test 6: Mixed DCL + MACRODEF in same .msw include (GitHub #2) */
    uart_puts("--- include: mixed DCL + MACRODEF ---");
    inc_init();
    inc_register("MIXED", "DCL MVAR INT(24); MACRODEF MTEST; REQUIRED PTR(expr); GEN DO; 'mvi r0,0'; END; END;");

    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();
    mac_init();

    parse_init("%INCLUDE MIXED; MAIN: PROC; MVAR = 1; END;");
    prog = parse_program();
    if (parse_err) {
        uart_putstr("  FAIL parse: ");
        uart_puts(parse_errmsg);
        errs = errs + 1;
    } else {
        uart_puts("  OK: mixed DCL + MACRODEF parsed");
        /* Verify the macro was registered */
        if (mac_count < 1) {
            uart_puts("  FAIL: macro not registered");
            errs = errs + 1;
        } else if (!str_eq_nocase(mac_name(0), "MTEST")) {
            uart_puts("  FAIL: wrong macro name");
            errs = errs + 1;
        } else {
            uart_puts("  OK: MTEST macro registered");
        }
    }

    /* Summary */
    uart_putstr("include processing errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_macro_def(void) {
    int errs;
    int mi;
    int idx;
    int lc_idx;
    int count;
    char *src;

    errs = 0;

    /* Test 1: Parse GETMAIN macro with REQUIRED/OPTIONAL clauses and GEN block */
    uart_puts("--- macrodef: GETMAIN ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   OPTIONAL SUBPOOL(expr);"
        "   OPTIONAL ADDRESS(lvalue);"
        "   OPTIONAL RC(lvalue);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "      'svc 10';"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    if (mac_parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(mac_parse_errmsg);
        errs = errs + 1;
    } else if (mi < 0) {
        uart_puts("  FAIL: mac_parse_def returned -1");
        errs = errs + 1;
    } else {
        /* Verify macro name */
        if (!str_eq_nocase(mac_name(mi), "GETMAIN")) {
            uart_puts("  FAIL: name != GETMAIN");
            errs = errs + 1;
        } else {
            uart_puts("  OK: name = GETMAIN");
        }

        /* Verify clause count */
        if (mac_cl_count[mi] != 4) {
            uart_putstr("  FAIL: clause count = ");
            print_int(mac_cl_count[mi]);
            uart_puts(" (expected 4)");
            errs = errs + 1;
        } else {
            uart_puts("  OK: 4 clauses");
        }

        /* Verify first clause: REQUIRED LENGTH(expr) */
        idx = mac_cl_idx(mi, 0);
        if (mac_cl_req[idx] != MCLAUSE_REQUIRED ||
            !str_eq_nocase(mac_cl_name(mi, 0), "LENGTH") ||
            mac_cl_types[idx] != MCLAUSE_EXPR) {
            uart_puts("  FAIL: clause 0 wrong");
            errs = errs + 1;
        } else {
            uart_puts("  OK: REQUIRED LENGTH(expr)");
        }

        /* Verify third clause: OPTIONAL ADDRESS(lvalue) */
        idx = mac_cl_idx(mi, 2);
        if (mac_cl_req[idx] != MCLAUSE_OPTIONAL ||
            !str_eq_nocase(mac_cl_name(mi, 2), "ADDRESS") ||
            mac_cl_types[idx] != MCLAUSE_LVALUE) {
            uart_puts("  FAIL: clause 2 wrong");
            errs = errs + 1;
        } else {
            uart_puts("  OK: OPTIONAL ADDRESS(lvalue)");
        }

        /* Verify GEN block */
        if (mac_gen_count[mi] != 1) {
            uart_putstr("  FAIL: gen count = ");
            print_int(mac_gen_count[mi]);
            uart_puts(" (expected 1)");
            errs = errs + 1;
        } else {
            uart_puts("  OK: 1 GEN block");
        }

        lc_idx = mac_gen_lc_idx(mi, 0);
        if (mac_gen_lcount[lc_idx] != 2) {
            uart_putstr("  FAIL: gen line count = ");
            print_int(mac_gen_lcount[lc_idx]);
            uart_puts(" (expected 2)");
            errs = errs + 1;
        } else {
            uart_puts("  OK: 2 GEN lines");
        }

        /* Verify GEN line content */
        if (!str_eq(mac_gen_line(mi, 0, 0), "mvi r0,{LENGTH}")) {
            uart_putstr("  FAIL: gen line 0 = '");
            uart_putstr(mac_gen_line(mi, 0, 0));
            uart_puts("'");
            errs = errs + 1;
        } else {
            uart_puts("  OK: gen line 0 = 'mvi r0,{LENGTH}'");
        }

        if (!str_eq(mac_gen_line(mi, 0, 1), "svc 10")) {
            uart_putstr("  FAIL: gen line 1 = '");
            uart_putstr(mac_gen_line(mi, 0, 1));
            uart_puts("'");
            errs = errs + 1;
        } else {
            uart_puts("  OK: gen line 1 = 'svc 10'");
        }

        mac_dump();
    }

    /* Test 2: Lookup by name */
    uart_puts("--- macrodef: lookup ---");
    if (mac_lookup("GETMAIN") != 0) {
        uart_puts("  FAIL: GETMAIN not found at index 0");
        errs = errs + 1;
    } else {
        uart_puts("  OK: GETMAIN found at index 0");
    }
    if (mac_lookup("NONEXIST") != -1) {
        uart_puts("  FAIL: NONEXIST should return -1");
        errs = errs + 1;
    } else {
        uart_puts("  OK: NONEXIST returns -1");
    }

    /* Test 3: Parse multiple macros from .msw file */
    uart_puts("--- macrodef: multi ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "      'svc 10';"
        "   END;"
        "END;"
        "MACRODEF FREEMAIN;"
        "   REQUIRED ADDRESS(expr);"
        "   GEN DO;"
        "      'mvi r0,{ADDRESS}';"
        "      'svc 11';"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    count = mac_parse_file();

    if (mac_parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(mac_parse_errmsg);
        errs = errs + 1;
    } else if (count != 2) {
        uart_putstr("  FAIL: parsed ");
        print_int(count);
        uart_puts(" macros (expected 2)");
        errs = errs + 1;
    } else {
        uart_puts("  OK: parsed 2 macros");
    }

    if (mac_lookup("GETMAIN") != 0) {
        uart_puts("  FAIL: GETMAIN not at index 0");
        errs = errs + 1;
    } else {
        uart_puts("  OK: GETMAIN at index 0");
    }
    if (mac_lookup("FREEMAIN") != 1) {
        uart_puts("  FAIL: FREEMAIN not at index 1");
        errs = errs + 1;
    } else {
        uart_puts("  OK: FREEMAIN at index 1");
    }

    mac_dump();

    /* Test 4: Macro with body (IF/THEN statements after GEN) */
    uart_puts("--- macrodef: body ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   OPTIONAL ADDRESS(lvalue);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "      'svc 10';"
        "   END;"
        "   IF ADDRESS THEN DO;"
        "      ADDRESS = 0;"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    if (mac_parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(mac_parse_errmsg);
        errs = errs + 1;
    } else {
        if (mac_body(mi)[0] == 0) {
            uart_puts("  FAIL: body is empty");
            errs = errs + 1;
        } else {
            uart_putstr("  OK: body = '");
            uart_putstr(mac_body(mi));
            uart_puts("'");
        }
        if (!str_find(mac_body(mi), "IF")) {
            uart_puts("  FAIL: body missing IF");
            errs = errs + 1;
        } else {
            uart_puts("  OK: body contains IF");
        }
    }

    /* Test 5: label clause type */
    uart_puts("--- macrodef: label clause ---");
    mac_init();

    src = "MACRODEF BRANCH;"
        "   REQUIRED TARGET(label);"
        "   OPTIONAL COND(expr);"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    if (mac_parse_err) {
        uart_putstr("  PARSE ERROR: ");
        uart_puts(mac_parse_errmsg);
        errs = errs + 1;
    } else {
        idx = mac_cl_idx(mi, 0);
        if (mac_cl_types[idx] != MCLAUSE_LABEL) {
            uart_puts("  FAIL: TARGET type != label");
            errs = errs + 1;
        } else {
            uart_puts("  OK: TARGET(label)");
        }
        idx = mac_cl_idx(mi, 1);
        if (mac_cl_types[idx] != MCLAUSE_EXPR) {
            uart_puts("  FAIL: COND type != expr");
            errs = errs + 1;
        } else {
            uart_puts("  OK: COND(expr)");
        }
    }

    /* Summary */
    uart_putstr("macrodef parsing errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_macro_expand(void) {
    int errs;
    int mi;
    char *result;
    char *src;

    errs = 0;

    /* Test 1: Basic GETMAIN expansion with LENGTH and ADDRESS */
    uart_puts("--- expand: GETMAIN(LENGTH(256), ADDRESS(BUF)) ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   OPTIONAL SUBPOOL(expr);"
        "   OPTIONAL ADDRESS(lvalue);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "      'svc 10';"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();
    if (mi < 0 || mac_parse_err) {
        uart_puts("  FAIL: could not parse GETMAIN def");
        errs = errs + 1;
    } else {
        /* Now invoke: ?GETMAIN(LENGTH(256), ADDRESS(BUF)) */
        src = "?GETMAIN(LENGTH(256), ADDRESS(BUF))";
        lex_init(src, str_len(src));
        lex_scan(); /* gets TOK_QUESTION */

        result = mac_invoke();
        if (!result) {
            uart_putstr("  FAIL: mac_invoke returned null: ");
            uart_puts(mac_expand_errmsg);
            errs = errs + 1;
        } else {
            uart_putstr("  expansion: '");
            uart_putstr(result);
            uart_puts("'");

            /* Should contain ASM DO with 'mvi r0,256' */
            if (!str_find(result, "mvi r0,256")) {
                uart_puts("  FAIL: missing 'mvi r0,256'");
                errs = errs + 1;
            } else {
                uart_puts("  OK: contains 'mvi r0,256'");
            }

            /* Should contain 'svc 10' */
            if (!str_find(result, "svc 10")) {
                uart_puts("  FAIL: missing 'svc 10'");
                errs = errs + 1;
            } else {
                uart_puts("  OK: contains 'svc 10'");
            }

            /* Should contain ASM DO wrapper */
            if (!str_find(result, "ASM DO")) {
                uart_puts("  FAIL: missing ASM DO wrapper");
                errs = errs + 1;
            } else {
                uart_puts("  OK: contains ASM DO wrapper");
            }
        }
    }

    /* Test 2: Missing required keyword */
    uart_puts("--- expand: missing required ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    src = "?GETMAIN()";
    lex_init(src, str_len(src));
    lex_scan();

    result = mac_invoke();
    if (result) {
        uart_puts("  FAIL: should have failed for missing REQUIRED");
        errs = errs + 1;
    } else {
        uart_puts("  OK: rejected missing required keyword");
    }

    /* Test 3: Unknown macro */
    uart_puts("--- expand: unknown macro ---");
    src = "?UNKNOWN(FOO(1))";
    lex_init(src, str_len(src));
    lex_scan();

    result = mac_invoke();
    if (result) {
        uart_puts("  FAIL: should have failed for unknown macro");
        errs = errs + 1;
    } else {
        uart_puts("  OK: rejected unknown macro");
    }

    /* Test 4: Macro with body substitution */
    uart_puts("--- expand: body substitution ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   OPTIONAL ADDRESS(lvalue);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "      'svc 10';"
        "   END;"
        "   IF ADDRESS THEN DO;"
        "      ADDRESS = 0;"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    src = "?GETMAIN(LENGTH(128), ADDRESS(BUF))";
    lex_init(src, str_len(src));
    lex_scan();

    result = mac_invoke();
    if (!result) {
        uart_putstr("  FAIL: mac_invoke returned null: ");
        uart_puts(mac_expand_errmsg);
        errs = errs + 1;
    } else {
        uart_putstr("  expansion: '");
        uart_putstr(result);
        uart_puts("'");

        /* GEN should have 128 substituted */
        if (!str_find(result, "mvi r0,128")) {
            uart_puts("  FAIL: missing 'mvi r0,128'");
            errs = errs + 1;
        } else {
            uart_puts("  OK: GEN has 'mvi r0,128'");
        }

        /* Body should have BUF substituted for ADDRESS */
        if (!str_find(result, "BUF")) {
            uart_puts("  FAIL: missing BUF in body");
            errs = errs + 1;
        } else {
            uart_puts("  OK: body contains BUF");
        }

        /* Body should have IF */
        if (!str_find(result, "IF")) {
            uart_puts("  FAIL: missing IF in body");
            errs = errs + 1;
        } else {
            uart_puts("  OK: body contains IF");
        }
    }

    /* Test 5: Multiple GEN blocks */
    uart_puts("--- expand: multiple GEN ---");
    mac_init();

    src = "MACRODEF SVC;"
        "   REQUIRED CODE(expr);"
        "   REQUIRED TARGET(expr);"
        "   GEN DO;"
        "      'mvi r0,{CODE}';"
        "   END;"
        "   GEN DO;"
        "      'la r1,{TARGET}';"
        "      'svc 0';"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    src = "?SVC(CODE(42), TARGET(4096))";
    lex_init(src, str_len(src));
    lex_scan();

    result = mac_invoke();
    if (!result) {
        uart_putstr("  FAIL: mac_invoke returned null: ");
        uart_puts(mac_expand_errmsg);
        errs = errs + 1;
    } else {
        uart_putstr("  expansion: '");
        uart_putstr(result);
        uart_puts("'");

        if (!str_find(result, "mvi r0,42")) {
            uart_puts("  FAIL: missing 'mvi r0,42'");
            errs = errs + 1;
        } else {
            uart_puts("  OK: contains 'mvi r0,42'");
        }

        if (!str_find(result, "la r1,4096")) {
            uart_puts("  FAIL: missing 'la r1,4096'");
            errs = errs + 1;
        } else {
            uart_puts("  OK: contains 'la r1,4096'");
        }
    }

    /* Test 6: Optional clause not provided */
    uart_puts("--- expand: optional not provided ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   OPTIONAL SUBPOOL(expr);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "      'svc 10';"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    src = "?GETMAIN(LENGTH(512))";
    lex_init(src, str_len(src));
    lex_scan();

    result = mac_invoke();
    if (!result) {
        uart_putstr("  FAIL: mac_invoke returned null: ");
        uart_puts(mac_expand_errmsg);
        errs = errs + 1;
    } else {
        uart_putstr("  expansion: '");
        uart_putstr(result);
        uart_puts("'");

        if (!str_find(result, "mvi r0,512")) {
            uart_puts("  FAIL: missing 'mvi r0,512'");
            errs = errs + 1;
        } else {
            uart_puts("  OK: contains 'mvi r0,512'");
        }
    }

    /* Test 7: Feed expansion back through lexer */
    uart_puts("--- expand: re-lex expansion ---");
    mac_init();

    src = "MACRODEF GETMAIN;"
        "   REQUIRED LENGTH(expr);"
        "   GEN DO;"
        "      'mvi r0,{LENGTH}';"
        "   END;"
        "END;";

    lex_init(src, str_len(src));
    lex_scan();
    mi = mac_parse_def();

    src = "?GETMAIN(LENGTH(256))";
    lex_init(src, str_len(src));
    lex_scan();

    result = mac_invoke();
    if (!result) {
        uart_putstr("  FAIL: mac_invoke returned null: ");
        uart_puts(mac_expand_errmsg);
        errs = errs + 1;
    } else {
        /* Re-lex the expansion and check tokens */
        lex_init(result, str_len(result));
        lex_scan();
        if (cur_type != TOK_ASM) {
            uart_putstr("  FAIL: first token not ASM, got ");
            uart_puts(tok_name(cur_type));
            errs = errs + 1;
        } else {
            uart_puts("  OK: first token is ASM");
        }
        lex_scan();
        if (cur_type != TOK_DO) {
            uart_putstr("  FAIL: second token not DO, got ");
            uart_puts(tok_name(cur_type));
            errs = errs + 1;
        } else {
            uart_puts("  OK: second token is DO");
        }
        lex_scan(); /* ';' */
        lex_scan(); /* string literal */
        if (cur_type != TOK_STRING) {
            uart_putstr("  FAIL: expected STRING, got ");
            uart_puts(tok_name(cur_type));
            errs = errs + 1;
        } else if (!str_find(cur_text, "mvi r0,256")) {
            uart_putstr("  FAIL: string = '");
            uart_putstr(cur_text);
            uart_puts("'");
            errs = errs + 1;
        } else {
            uart_puts("  OK: string = 'mvi r0,256'");
        }
    }

    /* Summary */
    uart_putstr("macro expansion errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_conditional(void) {
    int errs = 0;
    char *src;

    /* Test 1: %DEFINE and DEFINED() condition */
    uart_puts("Test 1: %DEFINE and %IF DEFINED");
    arena_init();
    src = "%DEFINE TARGET_COR24;"
          "%IF DEFINED(TARGET_COR24);"
          "DCL A INT(24);"
          "%ENDIF;";
    lex_init(src, str_len(src));
    lex_scan();
    if (cur_type != TOK_DCL) {
        uart_putstr("  FAIL: expected DCL, got ");
        uart_puts(tok_name(cur_type));
        errs = errs + 1;
    } else {
        uart_puts("  OK: true branch emits DCL");
    }

    /* Test 2: false branch is skipped */
    uart_puts("Test 2: false %IF branch skipped");
    arena_init();
    src = "%IF DEFINED(NONEXISTENT);"
          "DCL HIDDEN INT(24);"
          "%ENDIF;"
          "DCL VISIBLE BYTE;";
    lex_init(src, str_len(src));
    lex_scan();
    if (cur_type != TOK_DCL) {
        uart_putstr("  FAIL: expected DCL, got ");
        uart_puts(tok_name(cur_type));
        errs = errs + 1;
    } else {
        lex_scan(); /* VISIBLE */
        if (!str_eq_nocase(cur_text, "VISIBLE")) {
            uart_putstr("  FAIL: expected VISIBLE, got ");
            uart_puts(cur_text);
            errs = errs + 1;
        } else {
            uart_puts("  OK: false branch skipped, got VISIBLE");
        }
    }

    /* Test 3: %IF/%ELSE */
    uart_puts("Test 3: %IF/%ELSE");
    arena_init();
    src = "%DEFINE MODE DEBUG;"
          "%IF MODE = RELEASE;"
          "DCL WRONG INT(24);"
          "%ELSE;"
          "DCL RIGHT INT(24);"
          "%ENDIF;";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* RIGHT */
    if (!str_eq_nocase(cur_text, "RIGHT")) {
        uart_putstr("  FAIL: expected RIGHT, got ");
        uart_puts(cur_text);
        errs = errs + 1;
    } else {
        uart_puts("  OK: %ELSE branch taken, got RIGHT");
    }

    /* Test 4: %IF with value equality -- true case */
    uart_puts("Test 4: %IF value equality (true)");
    arena_init();
    src = "%DEFINE TARGET COR24;"
          "%IF TARGET = COR24;"
          "DCL MATCH INT(24);"
          "%ENDIF;";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* MATCH */
    if (!str_eq_nocase(cur_text, "MATCH")) {
        uart_putstr("  FAIL: expected MATCH, got ");
        uart_puts(cur_text);
        errs = errs + 1;
    } else {
        uart_puts("  OK: value equality true, got MATCH");
    }

    /* Test 5: nested %IF */
    uart_puts("Test 5: nested %IF");
    arena_init();
    src = "%DEFINE OUTER_FLAG;"
          "%IF DEFINED(OUTER_FLAG);"
            "%DEFINE INNER_FLAG;"
            "%IF DEFINED(INNER_FLAG);"
              "DCL DEEP INT(24);"
            "%ENDIF;"
          "%ENDIF;";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* DEEP */
    if (!str_eq_nocase(cur_text, "DEEP")) {
        uart_putstr("  FAIL: expected DEEP, got ");
        uart_puts(cur_text);
        errs = errs + 1;
    } else {
        uart_puts("  OK: nested %IF, got DEEP");
    }

    /* Test 6: nested %IF with false outer */
    uart_puts("Test 6: nested %IF with false outer");
    arena_init();
    src = "%IF DEFINED(NOPE);"
            "%IF DEFINED(ALSO_NOPE);"
              "DCL HIDDEN INT(24);"
            "%ENDIF;"
          "%ENDIF;"
          "DCL FOUND BYTE;";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* FOUND */
    if (!str_eq_nocase(cur_text, "FOUND")) {
        uart_putstr("  FAIL: expected FOUND, got ");
        uart_puts(cur_text);
        errs = errs + 1;
    } else {
        uart_puts("  OK: false outer skips all, got FOUND");
    }

    /* Test 7: %DEFINE with value, bare name condition */
    uart_puts("Test 7: bare name condition (shorthand for DEFINED)");
    arena_init();
    src = "%DEFINE FEATURE;"
          "%IF FEATURE;"
          "DCL OK INT(24);"
          "%ENDIF;";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* OK */
    if (!str_eq_nocase(cur_text, "OK")) {
        uart_putstr("  FAIL: expected OK, got ");
        uart_puts(cur_text);
        errs = errs + 1;
    } else {
        uart_puts("  OK: bare name condition works");
    }

    /* Test 8: %IF with %INCLUDE interaction */
    uart_puts("Test 8: %IF with %INCLUDE");
    arena_init();
    inc_init();
    inc_register("CORE", "DCL CORE_VAR INT(24);");
    src = "%DEFINE USE_CORE;"
          "%IF DEFINED(USE_CORE);"
          "%INCLUDE CORE;"
          "%ENDIF;";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* CORE_VAR */
    if (!str_eq_nocase(cur_text, "CORE_VAR")) {
        uart_putstr("  FAIL: expected CORE_VAR, got ");
        uart_puts(cur_text);
        errs = errs + 1;
    } else {
        uart_puts("  OK: conditional include works");
    }

    /* Test 9: %ELSE when %IF was true -- else skipped */
    uart_puts("Test 9: %ELSE skipped when %IF true");
    arena_init();
    src = "%DEFINE FLAG;"
          "%IF DEFINED(FLAG);"
          "DCL YES INT(24);"
          "%ELSE;"
          "DCL NO INT(24);"
          "%ENDIF;"
          "DCL AFTER BYTE;";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* YES */
    if (!str_eq_nocase(cur_text, "YES")) {
        uart_putstr("  FAIL: expected YES, got ");
        uart_puts(cur_text);
        errs = errs + 1;
    } else {
        lex_scan(); /* INT */
        lex_scan(); /* ( */
        lex_scan(); /* 24 */
        lex_scan(); /* ) */
        lex_scan(); /* ; */
        lex_scan(); /* DCL (AFTER) */
        lex_scan(); /* AFTER */
        if (!str_eq_nocase(cur_text, "AFTER")) {
            uart_putstr("  FAIL: expected AFTER, got ");
            uart_puts(cur_text);
            errs = errs + 1;
        } else {
            uart_puts("  OK: %ELSE skipped, got AFTER");
        }
    }

    /* Test 10: %DEFINE value substitution in expressions */
    uart_puts("Test 10: %DEFINE value substitution (numeric)");
    arena_init();
    src = "%DEFINE TAG_NULL 0;"
          "TAG_NULL";
    lex_init(src, str_len(src));
    lex_scan(); /* TAG_NULL -> should become TOK_NUM with value 0 */
    if (cur_type != TOK_NUM || cur_ival != 0) {
        uart_putstr("  FAIL: expected TOK_NUM 0, got type=");
        print_int(cur_type);
        uart_putstr(" val=");
        print_int(cur_ival);
        uart_putchar(10);
        errs = errs + 1;
    } else {
        uart_puts("  OK: TAG_NULL -> TOK_NUM 0");
    }

    /* Test 11: %DEFINE value substitution (text/keyword) */
    uart_puts("Test 11: %DEFINE value substitution (text)");
    arena_init();
    src = "%DEFINE MYTYPE INT;"
          "DCL A MYTYPE(24);";
    lex_init(src, str_len(src));
    lex_scan(); /* DCL */
    lex_scan(); /* A */
    lex_scan(); /* MYTYPE -> should become INT keyword */
    if (cur_type != TOK_INT) {
        uart_putstr("  FAIL: expected TOK_INT, got ");
        uart_puts(tok_name(cur_type));
        errs = errs + 1;
    } else {
        uart_puts("  OK: MYTYPE -> TOK_INT");
    }

    /* Test 12: %DEFINE with larger numeric value */
    uart_puts("Test 12: %DEFINE numeric value 255");
    arena_init();
    src = "%DEFINE BUFSIZE 255;"
          "BUFSIZE";
    lex_init(src, str_len(src));
    lex_scan(); /* BUFSIZE -> 255 */
    if (cur_type != TOK_NUM || cur_ival != 255) {
        uart_putstr("  FAIL: expected TOK_NUM 255, got type=");
        print_int(cur_type);
        uart_putstr(" val=");
        print_int(cur_ival);
        uart_putchar(10);
        errs = errs + 1;
    } else {
        uart_puts("  OK: BUFSIZE -> TOK_NUM 255");
    }

    /* Test 13: %DEFINE with no value does NOT substitute */
    uart_puts("Test 13: %DEFINE flag-only (no substitution)");
    arena_init();
    src = "%DEFINE FLAG;"
          "FLAG";
    lex_init(src, str_len(src));
    lex_scan(); /* FLAG -> should remain TOK_IDENT */
    if (cur_type != TOK_IDENT) {
        uart_putstr("  FAIL: expected TOK_IDENT, got ");
        uart_puts(tok_name(cur_type));
        errs = errs + 1;
    } else {
        uart_puts("  OK: FLAG remains TOK_IDENT (flag-only define)");
    }

    /* Summary */
    uart_putstr("conditional compilation errors: ");
    print_int(errs);
    uart_putchar(10);
}

/* --- Hello World end-to-end compilation --- */

/* COR24 runtime preamble: _start entry point */
void emit_runtime_start(void) {
    emit_line("        .text");
    emit_nl();
    emit_line("        .globl  _start");
    emit_line("_start:");
    emit_line("        la      r0,_MAIN");
    emit_line("        jal     r1,(r0)");
    emit_line("_halt:");
    emit_line("        bra     _halt");
    emit_nl();
}

/* COR24 UART_PUTCHAR runtime: write byte to UART */
void emit_runtime_uart_putchar(void) {
    emit_line("        .globl  _UART_PUTCHAR");
    emit_line("_UART_PUTCHAR:");
    emit_line("        push    fp");
    emit_line("        push    r2");
    emit_line("        push    r1");
    emit_line("        mov     fp,sp");
    /* Busy-wait: poll TX busy (bit 7 of status at 0xFF0101) */
    emit_line("_uart_tx_wait:");
    emit_line("        la      r2,16711937");
    emit_line("        lbu     r0,0(r2)");
    emit_line("        lcu     r1,128");
    emit_line("        and     r0,r1");
    emit_line("        ceq     r0,z");
    emit_line("        brf     _uart_tx_wait");
    /* Write byte to UART data register (0xFF0100) */
    emit_line("        la      r2,16711936");
    emit_line("        lw      r0,9(fp)");
    emit_line("        sb      r0,0(r2)");
    emit_line("        mov     sp,fp");
    emit_line("        pop     r1");
    emit_line("        pop     r2");
    emit_line("        pop     fp");
    emit_line("        jmp     (r1)");
    emit_nl();
}

/* COR24 UART_PUTS runtime: print null-terminated string + newline */
void emit_runtime_uart_puts(void) {
    emit_line("        .globl  _UART_PUTS");
    emit_line("_UART_PUTS:");
    emit_line("        push    fp");
    emit_line("        push    r2");
    emit_line("        push    r1");
    emit_line("        mov     fp,sp");
    emit_line("        lw      r2,9(fp)");
    emit_line("_uart_puts_loop:");
    emit_line("        lbu     r0,0(r2)");
    emit_line("        ceq     r0,z");
    emit_line("        brt     _uart_puts_done");
    /* call UART_PUTCHAR(r0) -- save r2, push arg, call, clean, restore */
    emit_line("        push    r2");
    emit_line("        push    r0");
    emit_line("        la      r0,_UART_PUTCHAR");
    emit_line("        jal     r1,(r0)");
    emit_line("        add     sp,3");
    emit_line("        pop     r2");
    emit_line("        lc      r0,1");
    emit_line("        add     r2,r0");
    emit_line("        bra     _uart_puts_loop");
    emit_line("_uart_puts_done:");
    /* print newline */
    emit_line("        lc      r0,10");
    emit_line("        push    r0");
    emit_line("        la      r0,_UART_PUTCHAR");
    emit_line("        jal     r1,(r0)");
    emit_line("        add     sp,3");
    emit_line("        mov     sp,fp");
    emit_line("        pop     r1");
    emit_line("        pop     r2");
    emit_line("        pop     fp");
    emit_line("        jmp     (r1)");
    emit_nl();
}

/* Compile a PL/SW source string to a complete .s program.
 * Returns the assembly output string, or 0 on error. */
char *compile_program(char *source) {
    int prog;

    /* Initialize all subsystems */
    arena_init();
    ast_init();
    sym_init();
    types_init();
    layout_init();
    emit_init();
    cg_init();
    cg_static_init();
    mac_init();

    /* Build source line table for listing */
    src_lines_init(source, str_len(source));

    /* Parse */
    parse_init(source);
    prog = parse_program();
    if (parse_err) {
        return 0;
    }

    /* Layout globals */
    layout_globals(prog);
    if (layout_err) {
        uart_putstr("STORAGE ERROR: ");
        uart_puts(layout_errmsg);
        return 0;
    }

    /* Emit runtime preamble */
    emit_runtime_start();
    emit_runtime_uart_putchar();
    emit_runtime_uart_puts();

    /* Emit user procedures */
    cg_program_procs(prog);
    if (cg_err) {
        return 0;
    }

    /* Emit static data */
    cg_emit_static_data(prog);

    /* Emit string literal table */
    cg_emit_string_table();

    return emit_output();
}

void test_hello_world(void) {
    int errs;
    char *src;
    char *out;

    errs = 0;

    /* The hello world PL/SW source */
    src = "DCL MSG(20) CHAR INIT('Hello from PL/SW!');"
          "MAIN: PROC;"
          "  CALL UART_PUTS(ADDR(MSG));"
          "END;";

    uart_puts("--- compiling hello.plsw ---");
    out = compile_program(src);
    if (!out) {
        uart_puts("  FAIL: compilation failed");
        errs = errs + 1;
    } else {
        /* Print the generated assembly */
        uart_puts("--- generated assembly ---");
        uart_putstr(out);
        uart_puts("--- end assembly ---");

        /* Verify key elements are present */
        if (!str_find(out, "_start:")) {
            uart_puts("  FAIL: missing _start");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has _start");
        }

        if (!str_find(out, "_UART_PUTCHAR:")) {
            uart_puts("  FAIL: missing _UART_PUTCHAR");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has _UART_PUTCHAR");
        }

        if (!str_find(out, "_UART_PUTS:")) {
            uart_puts("  FAIL: missing _UART_PUTS");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has _UART_PUTS");
        }

        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing _MAIN");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has _MAIN");
        }

        if (!str_find(out, "_MSG:")) {
            uart_puts("  FAIL: missing _MSG");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has _MSG");
        }

        if (!str_find(out, "la      r0,_MSG")) {
            uart_puts("  FAIL: missing ADDR(MSG) -> la r0,_MSG");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has ADDR(MSG) label ref");
        }

        if (!str_find(out, "la      r2,_UART_PUTS")) {
            uart_puts("  FAIL: missing CALL UART_PUTS");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has CALL UART_PUTS");
        }

        /* Check for Hello string bytes (H=72, e=101) */
        if (!str_find(out, ".byte   72")) {
            uart_puts("  FAIL: missing 'H' byte in MSG");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has 'H' byte (72)");
        }
    }

    /* Summary */
    uart_putstr("hello world compile errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_led_toggle(void) {
    int errs;
    char *src;
    char *out;

    errs = 0;

    /* LED toggle PL/SW source -- inline ASM + DO WHILE + MMIO */
    src = "DCL LED_STATE BYTE INIT(0);"
          "DCL COUNT INT;"
          "DELAY: PROC;"
          "  COUNT = 0;"
          "  DO WHILE (COUNT < 50000);"
          "    COUNT = COUNT + 1;"
          "  END;"
          "END;"
          "LED_WRITE: PROC(VAL BYTE);"
          "  ASM DO;"
          "    'la      r0,0xFF0000';"
          "    'lw      r1,9(fp)';"
          "    'sb      r1,0(r0)';"
          "  END;"
          "END;"
          "MAIN: PROC;"
          "  DCL I INT;"
          "  I = 0;"
          "  DO WHILE (I < 10);"
          "    IF (LED_STATE = 0) THEN"
          "      LED_STATE = 1;"
          "    ELSE"
          "      LED_STATE = 0;"
          "    CALL LED_WRITE(LED_STATE);"
          "    CALL DELAY();"
          "    I = I + 1;"
          "  END;"
          "  CALL LED_WRITE(0);"
          "END;";

    uart_puts("--- compiling led.plsw ---");
    out = compile_program(src);
    if (!out) {
        uart_puts("  FAIL: compilation failed");
        errs = errs + 1;
    } else {
        uart_puts("--- generated assembly ---");
        uart_putstr(out);
        uart_puts("--- end assembly ---");

        /* Verify inline ASM passthrough */
        if (!str_find(out, "la      r0,0xFF0000")) {
            uart_puts("  FAIL: missing LED MMIO address");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has LED MMIO address");
        }

        if (!str_find(out, "sb      r1,0(r0)")) {
            uart_puts("  FAIL: missing store byte to MMIO");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has store byte to MMIO");
        }

        /* Verify procedures */
        if (!str_find(out, "_DELAY:")) {
            uart_puts("  FAIL: missing DELAY proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has DELAY proc");
        }

        if (!str_find(out, "_LED_WRITE:")) {
            uart_puts("  FAIL: missing LED_WRITE proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has LED_WRITE proc");
        }

        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing MAIN proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has MAIN proc");
        }

        /* Verify DO WHILE loop structure */
        if (!str_find(out, "L") && str_find(out, "bra")) {
            uart_puts("  FAIL: missing loop structure");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has loop structure");
        }

        /* Verify CALL to LED_WRITE */
        if (!str_find(out, "la      r2,_LED_WRITE")) {
            uart_puts("  FAIL: missing CALL LED_WRITE");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has CALL LED_WRITE");
        }

        /* Verify CALL to DELAY */
        if (!str_find(out, "la      r2,_DELAY")) {
            uart_puts("  FAIL: missing CALL DELAY");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has CALL DELAY");
        }
    }

    uart_putstr("led toggle compile errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_counted_loop(void) {
    int errs;
    char *src;
    char *out;

    errs = 0;

    /* Counted loop PL/SW source -- DO I = 1 TO 10, PRINT_INT */
    src = "DCL DIGITS(12) CHAR;"
          "PRINT_INT: PROC(N INT);"
          "  DCL D INT;"
          "  DCL POS INT;"
          "  IF (N = 0) THEN DO;"
          "    CALL UART_PUTCHAR(48);"
          "    RETURN;"
          "  END;"
          "  POS = 0;"
          "  DO WHILE (N > 0);"
          "    D = N / 10;"
          "    DIGITS(POS) = N - D * 10 + 48;"
          "    N = D;"
          "    POS = POS + 1;"
          "  END;"
          "  DO WHILE (POS > 0);"
          "    POS = POS - 1;"
          "    CALL UART_PUTCHAR(DIGITS(POS));"
          "  END;"
          "END;"
          "MAIN: PROC;"
          "  DCL I INT;"
          "  DO I = 1 TO 10;"
          "    CALL PRINT_INT(I);"
          "    CALL UART_PUTCHAR(10);"
          "  END;"
          "END;";

    uart_puts("--- compiling loop.plsw ---");
    out = compile_program(src);
    if (!out) {
        uart_puts("  FAIL: compilation failed");
        errs = errs + 1;
    } else {
        uart_puts("--- generated assembly ---");
        uart_putstr(out);
        uart_puts("--- end assembly ---");

        /* Verify PRINT_INT procedure */
        if (!str_find(out, "_PRINT_INT:")) {
            uart_puts("  FAIL: missing PRINT_INT proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has PRINT_INT proc");
        }

        /* Verify MAIN procedure */
        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing MAIN proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has MAIN proc");
        }

        /* Verify DO count loop (call to PRINT_INT inside loop) */
        if (!str_find(out, "la      r2,_PRINT_INT")) {
            uart_puts("  FAIL: missing CALL PRINT_INT");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has CALL PRINT_INT");
        }

        /* Verify division used for digit extraction */
        if (!str_find(out, "__plsw_div")) {
            uart_puts("  FAIL: missing software div routine");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has software division");
        }

        /* Verify DIGITS array in data section */
        if (!str_find(out, "_DIGITS:")) {
            uart_puts("  FAIL: missing DIGITS array");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has DIGITS array");
        }

        /* Verify CALL UART_PUTCHAR */
        if (!str_find(out, "la      r2,_UART_PUTCHAR")) {
            uart_puts("  FAIL: missing CALL UART_PUTCHAR");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has CALL UART_PUTCHAR");
        }
    }

    uart_putstr("counted loop compile errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_record_pointer(void) {
    int errs;
    char *src;
    char *out;

    errs = 0;

    /* Record and pointer PL/SW source */
    src = "DCL DIGITS(12) CHAR;"
          "PRINT_INT: PROC(N INT);"
          "  DCL D INT;"
          "  DCL POS INT;"
          "  IF (N = 0) THEN DO;"
          "    CALL UART_PUTCHAR(48);"
          "    RETURN;"
          "  END;"
          "  POS = 0;"
          "  DO WHILE (N > 0);"
          "    D = N / 10;"
          "    DIGITS(POS) = N - D * 10 + 48;"
          "    N = D;"
          "    POS = POS + 1;"
          "  END;"
          "  DO WHILE (POS > 0);"
          "    POS = POS - 1;"
          "    CALL UART_PUTCHAR(DIGITS(POS));"
          "  END;"
          "END;"
          "MAIN: PROC;"
          "  DCL 1 POINT, 3 X INT, 3 Y INT;"
          "  DCL P PTR;"
          "  POINT.X = 100;"
          "  POINT.Y = 200;"
          "  CALL PRINT_INT(POINT.X);"
          "  CALL PRINT_INT(POINT.Y);"
          "  P = ADDR(POINT);"
          "  CALL PRINT_INT(P->X);"
          "  CALL PRINT_INT(P->Y);"
          "  CALL PRINT_INT(P->X + P->Y);"
          "END;";

    uart_puts("--- compiling record.plsw ---");
    out = compile_program(src);
    if (!out) {
        uart_puts("  FAIL: compilation failed");
        errs = errs + 1;
    } else {
        uart_puts("--- generated assembly ---");
        uart_putstr(out);
        uart_puts("--- end assembly ---");

        /* Verify MAIN procedure */
        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing MAIN proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has MAIN proc");
        }

        /* Verify PRINT_INT procedure */
        if (!str_find(out, "_PRINT_INT:")) {
            uart_puts("  FAIL: missing PRINT_INT proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has PRINT_INT proc");
        }

        /* Verify record field store (sw to computed offset) */
        if (!str_find(out, "sw      r0,0(r2)")) {
            uart_puts("  FAIL: missing record field store");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has record field store");
        }

        /* Verify ADDR operation (add r0,fp for local address) */
        if (!str_find(out, "add     r0,fp")) {
            uart_puts("  FAIL: missing ADDR (add r0,fp)");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has ADDR operation");
        }

        /* Verify pointer dereference load (lw r0,0(r2)) */
        if (!str_find(out, "lw      r0,0(r2)")) {
            uart_puts("  FAIL: missing ptr->field load");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has ptr->field load");
        }

        /* Verify addition for P->X + P->Y */
        if (!str_find(out, "add     r0,r1")) {
            uart_puts("  FAIL: missing add for field sum");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has field sum addition");
        }

        /* Verify calls to PRINT_INT */
        if (!str_find(out, "la      r2,_PRINT_INT")) {
            uart_puts("  FAIL: missing CALL PRINT_INT");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has CALL PRINT_INT");
        }
    }

    uart_putstr("record pointer compile errors: ");
    print_int(errs);
    uart_putchar(10);
}

void test_multi_based(void) {
    int errs;
    char *src;
    char *out;

    errs = 0;

    /* Two BASED records + PTR derefs to fields from both */
    src = "DCL 1 DESCR BASED,"
          "    3 DTAG BYTE,"
          "    3 DPAY PTR;"
          "DCL 1 OBJHDR BASED,"
          "    3 OKIND BYTE,"
          "    3 OSIZE INT;"
          "DCL D1(6) BYTE;"
          "DCL H1(6) BYTE;"
          "MAIN: PROC;"
          "  DCL DPTR PTR;"
          "  DCL HPTR PTR;"
          "  DPTR = ADDR(D1);"
          "  HPTR = ADDR(H1);"
          "  DPTR->DTAG = 3;"
          "  DPTR->DPAY = 0;"
          "  HPTR->OKIND = 1;"
          "  HPTR->OSIZE = 256;"
          "END;";

    uart_puts("--- compiling multi-based test ---");
    out = compile_program(src);
    if (!out) {
        uart_puts("  FAIL: compilation failed");
        errs = errs + 1;
    } else {
        uart_puts("  OK: compiled with two BASED records");

        /* Verify both DTAG and OKIND field accesses compiled */
        if (!str_find(out, "sb      r0,0(r2)")) {
            uart_puts("  FAIL: missing byte store for field access");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has byte store for field");
        }

        /* Verify MAIN proc exists */
        if (!str_find(out, "_MAIN:")) {
            uart_puts("  FAIL: missing MAIN proc");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has MAIN proc");
        }
    }

    /* Test 2: AND/OR with ptr->field comparisons (GitHub #5) */
    uart_puts("--- AND/OR with ptr->field ---");
    src = "DCL 1 DESCR BASED,"
          "    3 DTAG BYTE,"
          "    3 DPAY PTR;"
          "DCL D1(6) BYTE;"
          "MAIN: PROC;"
          "  DCL DPTR PTR;"
          "  DCL OK INT;"
          "  DPTR = ADDR(D1);"
          "  OK = (DPTR->DTAG = 0) AND (DPTR->DPAY = 0);"
          "  OK = (DPTR->DTAG = 3) OR (DPTR->DPAY != 0);"
          "END;";

    out = compile_program(src);
    if (!out) {
        uart_puts("  FAIL: AND/OR with deref failed to compile");
        errs = errs + 1;
    } else {
        uart_puts("  OK: AND/OR with ptr->field compiled");
        if (!str_find(out, "and     r0,r1")) {
            uart_puts("  FAIL: missing AND instruction");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has AND instruction");
        }
        if (!str_find(out, "or      r0,r1")) {
            uart_puts("  FAIL: missing OR instruction");
            errs = errs + 1;
        } else {
            uart_puts("  OK: has OR instruction");
        }
    }

    /* Test 3: BYTE param offsets (GitHub #7) */
    uart_puts("--- BYTE param stack offsets ---");
    src = "DCL 1 DESCR BASED,"
          "    3 DTAG BYTE,"
          "    3 DSUB BYTE,"
          "    3 DRSV BYTE,"
          "    3 DPAY PTR;"
          "DCL D1(6) BYTE;"
          "DESC_INIT: PROC(P PTR, T BYTE, S BYTE, V INT);"
          "  DCL DPTR PTR;"
          "  DPTR = P;"
          "  DPTR->DTAG = T;"
          "  DPTR->DSUB = S;"
          "  DPTR->DPAY = V;"
          "END;"
          "MAIN: PROC;"
          "  CALL DESC_INIT(ADDR(D1), 2, 0, 42);"
          "END;";

    out = compile_program(src);
    if (!out) {
        uart_puts("  FAIL: compilation failed");
        errs = errs + 1;
    } else {
        uart_puts("  OK: compiled with BYTE params");
        /* T at 12(fp), S at 15(fp), V at 18(fp) -- all 3-byte slots */
        if (!str_find(out, "lbu     r0,12(fp)")) {
            uart_puts("  FAIL: T should be at 12(fp)");
            errs = errs + 1;
        } else {
            uart_puts("  OK: T at 12(fp)");
        }
        if (!str_find(out, "lbu     r0,15(fp)")) {
            uart_puts("  FAIL: S should be at 15(fp)");
            errs = errs + 1;
        } else {
            uart_puts("  OK: S at 15(fp)");
        }
        if (!str_find(out, "lw      r0,18(fp)")) {
            uart_puts("  FAIL: V should be at 18(fp)");
            errs = errs + 1;
        } else {
            uart_puts("  OK: V at 18(fp)");
        }
    }

    uart_putstr("multi-based pointer errors: ");
    print_int(errs);
    uart_putchar(10);
}

/* Run a test suite by number. Returns 1 if valid suite. */
int run_suite(int n) {
    if (n == 0) { uart_puts("=== String Tests ==="); test_strings(); }
    else if (n == 1) { uart_puts("=== Arena Tests ==="); test_arena(); }
    else if (n == 2) { uart_puts("=== Token Tests ==="); test_tokens(); }
    else if (n == 3) { uart_puts("=== Lexer Tests ==="); test_lexer(); }
    else if (n == 4) { uart_puts("=== AST Tests ==="); test_ast(); }
    else if (n == 5) { uart_puts("=== DCL Parser Tests ==="); test_parser(); }
    else if (n == 6) { uart_puts("=== Expression Parser Tests ==="); test_expr_parser(); }
    else if (n == 7) { uart_puts("=== Statement Parser Tests ==="); test_stmt_parser(); }
    else if (n == 8) { uart_puts("=== Procedure Parser Tests ==="); test_proc_parser(); }
    else if (n == 9) { uart_puts("=== Top-Level Parser Tests ==="); test_toplevel_parser(); }
    else if (n == 10) { uart_puts("=== Symbol Table Tests ==="); test_symtab(); }
    else if (n == 11) { uart_puts("=== Type System Tests ==="); test_types(); }
    else if (n == 12) { uart_puts("=== Storage Layout Tests ==="); test_layout(); }
    else if (n == 13) { uart_puts("=== Emitter Framework Tests ==="); test_emit(); }
    else if (n == 14) { uart_puts("=== Expression Codegen Tests ==="); test_codegen(); }
    else if (n == 15) { uart_puts("=== Assignment Codegen Tests ==="); test_assign_codegen(); }
    else if (n == 16) { uart_puts("=== Static Data Tests ==="); test_static_data(); }
    else if (n == 17) { uart_puts("=== Procedure Codegen Tests ==="); test_proc_codegen(); }
    else if (n == 18) { uart_puts("=== Call Codegen Tests ==="); test_call_codegen(); }
    else if (n == 19) { uart_puts("=== Recursion Codegen Tests ==="); test_recursion_codegen(); }
    else if (n == 20) { uart_puts("=== IF Codegen Tests ==="); test_if_codegen(); }
    else if (n == 21) { uart_puts("=== DO WHILE Codegen Tests ==="); test_do_while_codegen(); }
    else if (n == 22) { uart_puts("=== DO COUNT Codegen Tests ==="); test_do_count_codegen(); }
    else if (n == 23) { uart_puts("=== Record Codegen Tests ==="); test_record_codegen(); }
    else if (n == 24) { uart_puts("=== Array Codegen Tests ==="); test_array_codegen(); }
    else if (n == 25) { uart_puts("=== Pointer Codegen Tests ==="); test_pointer_codegen(); }
    else if (n == 26) { uart_puts("=== Inline ASM Codegen Tests ==="); test_asm_codegen(); }
    else if (n == 27) { uart_puts("=== Include Processing Tests ==="); test_include(); }
    else if (n == 28) { uart_puts("=== Macro Definition Tests ==="); test_macro_def(); }
    else if (n == 29) { uart_puts("=== Macro Expansion Tests ==="); test_macro_expand(); }
    else if (n == 30) { uart_puts("=== Conditional Compilation Tests ==="); test_conditional(); }
    else if (n == 31) { uart_puts("=== Hello World Compile ==="); test_hello_world(); }
    else if (n == 32) { uart_puts("=== LED Toggle Compile ==="); test_led_toggle(); }
    else if (n == 33) { uart_puts("=== Counted Loop Compile ==="); test_counted_loop(); }
    else if (n == 34) { uart_puts("=== Record Pointer Compile ==="); test_record_pointer(); }
    else if (n == 35) { uart_puts("=== Multi-BASED Pointer Tests ==="); test_multi_based(); }
    else { return 0; }
    uart_puts("");
    return 1;
}

#define SUITE_COUNT 36

/* Source buffer for compile mode -- 8KB */
#define SRC_BUF_SIZE 8192
char src_buf[SRC_BUF_SIZE];

/* Include file buffer -- stores content for FILE: uploads */
#define INC_BUF_SIZE 32768
char inc_buf[INC_BUF_SIZE];
int inc_buf_top;

/* Allocate from include file buffer. Returns pointer or 0 on OOM. */
char *inc_buf_alloc(int n) {
    if (inc_buf_top + n > INC_BUF_SIZE) return 0;
    char *p = inc_buf + inc_buf_top;
    inc_buf_top = inc_buf_top + n;
    return p;
}

/* Read a line from UART into buf (up to maxlen-1 chars).
 * Stops at \n (not included), \x1E (record sep), \x04 (EOT), or \x03 (ETX).
 * Returns length, sets *term to the terminator character. */
int read_line_term(char *buf, int maxlen, int *term) {
    int pos = 0;
    int limit = maxlen - 1;

    while (pos < limit) {
        int ch = uart_getchar();
        if (ch == 10 || ch == 30 || ch == 4 || ch == 3) {
            *term = ch;
            break;
        }
        buf[pos] = ch;
        pos = pos + 1;
    }
    buf[pos] = 0;
    return pos;
}

/* Read content from UART until \x1E (record sep) or \x04 (EOT).
 * Stores into inc_buf_alloc'd memory. Returns pointer to content, or 0.
 * Sets *term to the terminator character. */
char *read_file_content(int *term) {
    int start = inc_buf_top;
    int ch;

    while (inc_buf_top < INC_BUF_SIZE - 1) {
        ch = uart_getchar();
        if (ch == 30 || ch == 4 || ch == 3) {
            *term = ch;
            inc_buf[inc_buf_top] = 0;
            inc_buf_top = inc_buf_top + 1;
            return inc_buf + start;
        }
        inc_buf[inc_buf_top] = ch;
        inc_buf_top = inc_buf_top + 1;
    }
    *term = 0;
    return 0;  /* overflow */
}

/* Read source from UART until EOF (0x04) or end-of-transmission (0x03).
 * Returns length of source read, or -1 on overflow. */
int read_source(char *buf, int maxlen) {
    int pos = 0;
    int limit = maxlen - 1;

    while (pos < limit) {
        int ch = uart_getchar();
        if (ch == 4 || ch == 3) {
            /* EOT or ETX -- end of source */
            break;
        }
        buf[pos] = ch;
        pos = pos + 1;
    }
    buf[pos] = 0;

    if (pos >= limit) return -1;
    return pos;
}

/* Read FILE: blocks and SOURCE: from UART.
 * Protocol:
 *   FILE:name\n<content>\x1E    -- register include file (repeatable)
 *   SOURCE:\n<content>\x04      -- main source to compile
 *   <raw content>\x04           -- legacy: no FILE:/SOURCE: prefix
 * Returns length of main source in src_buf, or -1 on error. */
int read_compile_input(void) {
    char hdr[80];
    int term;
    int hlen;
    char *name;
    char *content;
    int nlen;

    inc_buf_top = 0;
    inc_init();

    /* Peek at first line to detect protocol */
    hlen = read_line_term(hdr, 80, &term);

    /* Check for FILE: prefix */
    if (hlen >= 5 && hdr[0] == 70 && hdr[1] == 73 && hdr[2] == 76
                   && hdr[3] == 69 && hdr[4] == 58) {
        /* Protocol mode: FILE:/SOURCE: framing */
        while (1) {
            if (hlen >= 5 && hdr[0] == 70 && hdr[1] == 73 && hdr[2] == 76
                          && hdr[3] == 69 && hdr[4] == 58) {
                /* FILE:name -- register include file */
                nlen = hlen - 5;
                name = inc_buf_alloc(nlen + 1);
                if (!name) {
                    uart_puts("ERROR: include buffer overflow (name)");
                    return -1;
                }
                mem_copy(name, hdr + 5, nlen);
                name[nlen] = 0;

                content = read_file_content(&term);
                if (!content) {
                    uart_puts("ERROR: include buffer overflow (content)");
                    return -1;
                }

                inc_register(name, content);
                uart_putstr("  registered: ");
                uart_puts(name);

                if (term == 4 || term == 3) {
                    uart_puts("ERROR: unexpected EOT after FILE");
                    return -1;
                }

                /* Read next header line */
                hlen = read_line_term(hdr, 80, &term);

            } else if (hlen >= 7 && hdr[0] == 83 && hdr[1] == 79
                                 && hdr[2] == 85 && hdr[3] == 82
                                 && hdr[4] == 67 && hdr[5] == 69
                                 && hdr[6] == 58) {
                /* SOURCE: -- main source follows */
                return read_source(src_buf, SRC_BUF_SIZE);

            } else {
                uart_putstr("ERROR: expected FILE: or SOURCE:, got: ");
                uart_puts(hdr);
                return -1;
            }
        }
    } else {
        /* Legacy mode: no framing, first line is start of source */
        int pos = 0;
        int i = 0;
        /* Copy header line into src_buf */
        while (i < hlen && pos < SRC_BUF_SIZE - 2) {
            src_buf[pos] = hdr[i];
            pos = pos + 1;
            i = i + 1;
        }
        if (term == 10) {
            src_buf[pos] = 10;
            pos = pos + 1;
        }
        /* Read rest of source */
        while (pos < SRC_BUF_SIZE - 1) {
            int ch = uart_getchar();
            if (ch == 4 || ch == 3) break;
            src_buf[pos] = ch;
            pos = pos + 1;
        }
        src_buf[pos] = 0;
        if (pos >= SRC_BUF_SIZE - 1) return -1;
        return pos;
    }
}

int main() {
    char line[LINE_MAX];
    int suite;
    int len;

    uart_puts("PL/SW Compiler v0.1");
    uart_puts("COR24 target");
    uart_putstr("Enter suite # (0-34), 'a' for all, 'c' to compile, or 'r' for REPL: ");

    len = uart_getline(line, LINE_MAX);

    if (len > 0 && (line[0] == 65 || line[0] == 97)) {
        /* 'A' or 'a' -- run all suites */
        suite = 0;
        while (suite < SUITE_COUNT) {
            run_suite(suite);
            suite = suite + 1;
        }
    } else if (len > 0 && (line[0] == 67 || line[0] == 99)) {
        /* 'C' or 'c' -- compile mode */
        char *out;

        uart_puts("=== Compile mode ===");
        uart_puts("Send FILE:name / SOURCE: blocks, or raw source + EOT.");

        len = read_compile_input();
        if (len < 0) {
            uart_puts("ERROR: source read failed");
            return 1;
        }
        if (len == 0) {
            uart_puts("ERROR: empty source");
            return 1;
        }

        uart_puts("--- compiling ---");
        out = compile_program(src_buf);
        if (!out) {
            uart_puts("--- compilation failed ---");
            return 1;
        }
        uart_puts("--- generated assembly ---");
        uart_putstr(out);
        uart_puts("--- end assembly ---");
    } else if (len > 0 && (line[0] == 82 || line[0] == 114)) {
        /* 'R' or 'r' -- REPL */
        uart_puts("=== REPL (tokenizer) ===");
        while (1) {
            uart_putstr("> ");
            len = uart_getline(line, LINE_MAX);
            if (len == 0) continue;
            lex_init(line, str_len(line));
            while (1) {
                lex_scan();
                tok_print();
                if (cur_type == TOK_EOF) break;
            }
        }
    } else if (len > 0) {
        /* Parse number */
        suite = 0;
        int i = 0;
        while (i < len && line[i] >= 48 && line[i] <= 57) {
            suite = suite * 10 + (line[i] - 48);
            i = i + 1;
        }
        if (!run_suite(suite)) {
            uart_putstr("Unknown suite: ");
            print_int(suite);
            uart_putchar(10);
        }
    }

    return 0;
}
