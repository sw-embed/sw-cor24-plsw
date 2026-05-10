/* emit.h -- Assembly emitter framework for PL/SW compiler */
/* Manages output buffer, sections, labels, and instruction emission */

#ifndef EMIT_H
#define EMIT_H

#include "str.h"

/* --- Output buffer ---
 *
 * 4 KB coalescing buffer. PL/SW emits the .s as a forward-only
 * stream: when the buffer fills, emit_flush() drains it to UART
 * and resets emit_pos. The previous design accumulated the
 * entire compilation in a 256 KB buffer; that capped .s size at
 * 256 KB and consumed ~25% of COR24's 1 MB SRAM. Streaming
 * removes both ceilings.
 *
 * Coalescing (vs. per-character UART writes) keeps the syscall
 * count low: each emit_str / emit_int / emit_label is many
 * small emit_char calls, and cor24-emu's UART is char-at-a-time;
 * batching ~4 KB at a time amortises the overhead. */
#define EMIT_BUF_SIZE 4096

char emit_buf[EMIT_BUF_SIZE];
int emit_pos;     /* current write position within emit_buf */
int emit_err;     /* error flag (currently unused under streaming;
                   * reserved for future per-flush UART error
                   * propagation) */

/* Drain emit_buf[0..emit_pos] to UART and reset emit_pos.
 * Called automatically by emit_char when the buffer fills, and
 * manually at end-of-compilation to drain any tail. */
void emit_flush(void) {
    int i = 0;
    while (i < emit_pos) {
        uart_putchar(emit_buf[i]);
        i = i + 1;
    }
    emit_pos = 0;
}

/* --- Label counter --- */

int emit_label_next;  /* next unique label number */

/* --- Section tracking --- */

int emit_in_data;  /* 1 if currently in .data section, 0 for .text */

/* --- Indentation --- */

#define EMIT_INDENT "        "  /* 8 spaces, matching tc24r output */

/* --- Core emit functions --- */

void emit_init(void) {
    emit_pos = 0;
    emit_err = 0;
    emit_label_next = 0;
    emit_in_data = 0;
    mem_set(emit_buf, 0, 256);  /* clear start of buffer */
}

/* Append a single character to the output buffer.
 * When the buffer fills, drain it to UART via emit_flush()
 * and continue writing into the freshly-empty buffer. The .s
 * stream is unbounded by buffer size as a result. */
void emit_char(int ch) {
    if (emit_pos >= EMIT_BUF_SIZE - 1) {
        emit_flush();
    }
    emit_buf[emit_pos] = ch;
    emit_pos = emit_pos + 1;
}

/* Append a string to the output buffer */
void emit_str(char *s) {
    while (*s) {
        emit_char(*s);
        s = s + 1;
    }
}

/* Append a newline */
void emit_nl(void) {
    emit_char(10);
}

/* Append a string followed by newline */
void emit_line(char *s) {
    emit_str(s);
    emit_nl();
}

/* Emit an integer as decimal string */
void emit_int(int n) {
    char buf[12];
    int i;
    int neg;
    int d;

    if (n == 0) {
        emit_char(48);
        return;
    }

    neg = 0;
    if (n < 0) {
        neg = 1;
        n = 0 - n;
    }

    i = 0;
    while (n > 0) {
        d = n;
        /* manual modulo: d = n % 10 */
        while (d >= 10) {
            d = d - 10;
        }
        buf[i] = 48 + d;
        i = i + 1;
        n = n / 10;
    }

    if (neg) {
        emit_char(45);  /* '-' */
    }

    /* emit digits in reverse */
    while (i > 0) {
        i = i - 1;
        emit_char(buf[i]);
    }
}

/* --- Label generation --- */

/* Generate a unique label name into buf. Returns label number. */
int emit_new_label(void) {
    int n = emit_label_next;
    emit_label_next = emit_label_next + 1;
    return n;
}

/* Emit a label definition (e.g., "L42:") */
void emit_label(int label_num) {
    emit_char(76);  /* 'L' */
    emit_int(label_num);
    emit_char(58);  /* ':' */
    emit_nl();
}

/* Emit a label reference (e.g., "L42") without colon or newline */
void emit_label_ref(int label_num) {
    emit_char(76);  /* 'L' */
    emit_int(label_num);
}

/* Long branch helpers (unlimited range via la+jmp) */

/* Unconditional long branch: la r0,Lxx; jmp (r0) */
void emit_branch(int label_num) {
    emit_str(EMIT_INDENT);
    emit_str("la      r0,");
    emit_label_ref(label_num);
    emit_nl();
    emit_str(EMIT_INDENT);
    emit_line("jmp     (r0)");
}

/* Conditional long branch (true): brf _skip; la r0,Lxx; jmp (r0); _skip: */
void emit_branch_true(int label_num) {
    int skip = emit_new_label();
    emit_str(EMIT_INDENT);
    emit_str("brf     ");
    emit_label_ref(skip);
    emit_nl();
    emit_str(EMIT_INDENT);
    emit_str("la      r0,");
    emit_label_ref(label_num);
    emit_nl();
    emit_str(EMIT_INDENT);
    emit_line("jmp     (r0)");
    emit_label(skip);
}

/* Conditional long branch (false): brt _skip; la r0,Lxx; jmp (r0); _skip: */
void emit_branch_false(int label_num) {
    int skip = emit_new_label();
    emit_str(EMIT_INDENT);
    emit_str("brt     ");
    emit_label_ref(skip);
    emit_nl();
    emit_str(EMIT_INDENT);
    emit_str("la      r0,");
    emit_label_ref(label_num);
    emit_nl();
    emit_str(EMIT_INDENT);
    emit_line("jmp     (r0)");
    emit_label(skip);
}

/* Emit a named label (e.g., "_main:") */
void emit_named_label(char *name) {
    emit_char(95);  /* '_' */
    emit_str(name);
    emit_char(58);  /* ':' */
    emit_nl();
}

/* Emit a named label reference without colon */
void emit_named_ref(char *name) {
    emit_char(95);  /* '_' */
    emit_str(name);
}

/* --- Section management --- */

void emit_text_section(void) {
    if (emit_in_data) {
        emit_nl();
        emit_str(EMIT_INDENT);
        emit_line(".text");
        emit_in_data = 0;
    }
}

void emit_data_section(void) {
    if (!emit_in_data) {
        emit_nl();
        emit_str(EMIT_INDENT);
        emit_line(".data");
        emit_in_data = 1;
    }
}

/* --- Instruction emission --- */

/* Emit a .globl directive for a symbol */
void emit_global(char *name) {
    emit_str(EMIT_INDENT);
    emit_str(".globl  ");
    emit_named_ref(name);
    emit_nl();
}

/* Emit an instruction with no operands (e.g., "nop") */
void emit_instr0(char *op) {
    emit_str(EMIT_INDENT);
    emit_str(op);
    emit_nl();
}

/* Emit an instruction with one operand (e.g., "push fp") */
void emit_instr1(char *op, char *arg1) {
    emit_str(EMIT_INDENT);
    emit_str(op);
    emit_str("     ");
    emit_str(arg1);
    emit_nl();
}

/* Emit an instruction with two operands (e.g., "mov fp,sp") */
void emit_instr2(char *op, char *arg1, char *arg2) {
    emit_str(EMIT_INDENT);
    emit_str(op);
    emit_str("     ");
    emit_str(arg1);
    emit_char(44);  /* ',' */
    emit_str(arg2);
    emit_nl();
}

/* Emit a raw instruction line (already formatted) */
void emit_instr(char *line) {
    emit_str(EMIT_INDENT);
    emit_line(line);
}

/* --- Comment emission --- */

/* Emit a comment line (e.g., "; PL/SW: x = y + 1;") */
void emit_comment(char *text) {
    emit_str(EMIT_INDENT);
    emit_str("; ");
    emit_line(text);
}

/* Emit a source-line tracking comment */
void emit_source_line(char *source) {
    emit_comment(source);
}

/* --- Data emission helpers --- */

/* Emit a .byte directive */
void emit_byte(int val) {
    emit_str(EMIT_INDENT);
    emit_str(".byte   ");
    emit_int(val);
    emit_nl();
}

/* Emit a .word directive (24-bit on COR24) */
void emit_word(int val) {
    emit_str(EMIT_INDENT);
    emit_str(".word   ");
    emit_int(val);
    emit_nl();
}

/* Emit an ASCII string in .data section */
void emit_ascii(char *s) {
    emit_str(EMIT_INDENT);
    emit_str(".ascii  \"");
    emit_str(s);
    emit_line("\"");
}

/* Emit a zero byte (null terminator) */
void emit_zero(void) {
    emit_byte(0);
}

/* --- Procedure helpers --- */

/* Emit standard COR24 procedure prologue */
void emit_prologue(void) {
    emit_instr1("push", "fp");
    emit_instr1("push", "r2");
    emit_instr1("push", "r1");
    emit_instr2("mov", "fp", "sp");
}

/* Emit standard COR24 procedure epilogue */
void emit_epilogue(void) {
    emit_instr2("mov", "sp", "fp");
    emit_instr1("pop", "r1");
    emit_instr1("pop", "r2");
    emit_instr1("pop", "fp");
    emit_instr("jmp     (r1)");
}

/* --- Output access ---
 *
 * Both emit_output() and emit_length() reflect ONLY the
 * unflushed tail (the bytes in emit_buf since the last
 * emit_flush). Under the streaming design they're useful for:
 *
 *   * NULL-vs-non-NULL contract: compile_program returns
 *     emit_output() so callers can detect compile failure
 *     (returns 0/NULL) vs success (always non-NULL).
 *   * Compile-internal test suites in src/main.c that emit
 *     small fragments and inspect via str_find -- as long as
 *     the fragment is < EMIT_BUF_SIZE (4 KB), nothing flushes
 *     and the buffer holds the whole emission.
 *
 * Tests that emit > 4 KB will see only the trailing chunk.
 * Production callers (main()'s compile-mode handler) must NOT
 * print emit_output() after a compile -- the bytes already
 * streamed during the compile via emit_flush(); reprinting
 * the tail would duplicate the trailing < 4 KB to UART. */

/* Null-terminate the buffer and return pointer to the
 * unflushed tail. See note above re: streaming contract. */
char *emit_output(void) {
    if (emit_pos < EMIT_BUF_SIZE) {
        emit_buf[emit_pos] = 0;
    }
    return emit_buf;
}

/* Return current unflushed-tail length. See note on
 * emit_output() for the streaming contract. */
int emit_length(void) {
    return emit_pos;
}

#endif /* EMIT_H */
