#include "io.h"
#include "arena.h"
#include "lexer.h"
#include "ast.h"

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
