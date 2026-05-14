#include "io.h"
#include "arena.h"
#include "chunk.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "symtab.h"
#include "types.h"
#include "layout.h"
#include "emit.h"
#include "codegen.h"
#include "macro.h"
#include "compile.h"

#define LINE_MAX 128

/* Source buffer for compile mode -- 192 KiB (interim, per
   dcpls-enlarge-src-buf.md, 2026-05-12). Bumped from 65,536 to
   unblock dcsno's sno_engine.plsw consolidation (137,994 bytes
   single source). The overflow detector below stays as a
   tripwire if growth ever needs the next bump (the brief's
   long-term recommendation is 262,144 / 256 KiB). */
#define SRC_BUF_SIZE 196608
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
    int len;

    uart_puts("PL/SW Compiler v0.1");
    uart_puts("COR24 target");
    uart_putstr("Enter 'c' to compile or 'r' for REPL: ");

    len = uart_getline(line, LINE_MAX);

    if (len > 0 && (line[0] == 67 || line[0] == 99)) {
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

        uart_puts("--- generated assembly ---");
        out = compile_program(src_buf);
        if (!out) {
            uart_puts("--- compilation failed ---");
            return 1;
        }
        /* Streaming-emit contract: compile_program() streams the
         * .s to UART during compilation; we MUST NOT print
         * emit_output() afterwards or the trailing < 4 KB chunk
         * gets duplicated. */
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
    }

    return 0;
}
