/* compile.h -- Top-level compile_program() driver and the
 * COR24 runtime-preamble emitters it depends on. Shared by
 * src/main.c (production compiler binary) and src/test_main.c
 * (in-binary test runner) so both can call compile_program()
 * without duplicating the body or shuffling test code into the
 * production .lgo. */

#ifndef COMPILE_H
#define COMPILE_H

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

/* COR24 UART_GETCHAR runtime: poll UART RX, return byte in r0 */
void emit_runtime_uart_getchar(void) {
    emit_line("        .globl  _UART_GETCHAR");
    emit_line("_UART_GETCHAR:");
    emit_line("        push    fp");
    emit_line("        push    r2");
    emit_line("        push    r1");
    emit_line("        mov     fp,sp");
    /* Busy-wait: poll RX ready (bit 0 of status at 0xFF0101) */
    emit_line("_uart_rx_wait:");
    emit_line("        la      r2,16711937");
    emit_line("        lbu     r0,0(r2)");
    emit_line("        lcu     r1,1");
    emit_line("        and     r0,r1");
    emit_line("        ceq     r0,z");
    emit_line("        brt     _uart_rx_wait");
    /* Read byte from UART data register (0xFF0100); read clears RX ready */
    emit_line("        la      r2,16711936");
    emit_line("        lbu     r0,0(r2)");
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
    if (lex_fatal) {
        return 0;
    }

    /* Check for %DEFINE NOLISTING */
    if (def_defined("NOLISTING")) {
        cg_listing = 0;
    }

    /* Layout globals */
    layout_globals(prog);
    if (layout_err) {
        uart_putstr("STORAGE ERROR: ");
        uart_puts(layout_errmsg);
        return 0;
    }

    /* Emit runtime preamble (suppressed by %DEFINE LIBRARY) */
    if (!def_defined("LIBRARY")) {
        emit_runtime_start();
        emit_runtime_uart_putchar();
        emit_runtime_uart_getchar();
        emit_runtime_uart_puts();
    }

    /* Emit user procedures */
    cg_program_procs(prog);
    if (cg_err) {
        return 0;
    }

    /* Emit software division routine if any procedure used '/'.
     * Suppressed in LIBRARY mode -- library modules reference __plsw_div
     * as an external symbol resolved by the linker from the entry module. */
    if (!def_defined("LIBRARY")) {
        cg_emit_div_routine();
    }

    /* Emit static data */
    cg_emit_static_data(prog);

    /* Emit string literal table */
    cg_emit_string_table();

    /* Drain the streaming buffer's tail to UART. Bytes prior
     * to this already flowed during emit_char's auto-flush;
     * this just pushes whatever's left in emit_buf. Callers in
     * production (main()'s compile-mode handler) must NOT then
     * print emit_output() -- the streaming has already done
     * the work, and printing the tail again would duplicate
     * the last < 4 KB. emit_output() is still returned so
     * compile_program keeps its NULL-on-failure /
     * non-NULL-on-success contract, and so test suites that
     * inspect short emissions via str_find continue to work
     * for fragments that fit in the unflushed tail. */
    emit_flush();
    return emit_output();
}

#endif /* COMPILE_H */
