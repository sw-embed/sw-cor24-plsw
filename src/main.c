#include "io.h"
#include "arena.h"
#include "token.h"

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

    uart_puts("=== REPL ===");
    char line[LINE_MAX];

    while (1) {
        uart_putstr("> ");
        int len = uart_getline(line, LINE_MAX);

        if (len == 0) {
            continue;
        }

        /* echo back what was typed */
        uart_putstr("got ");
        print_int(len);
        uart_putstr(" chars: ");
        uart_puts(line);

        /* test parse_int on the input */
        int end = 0;
        int val = parse_int(line, 0, &end);
        if (end > 0) {
            uart_putstr("parsed int: ");
            print_int(val);
            uart_putchar(10);
        }
    }

    return 0;
}
