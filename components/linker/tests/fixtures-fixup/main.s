        .text

        .globl  _start
_start:
        la      r0,_ENTRY
        jal     r1,(r0)
_halt:
        bra     _halt

        .globl  _UART_PUTCHAR
_UART_PUTCHAR:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
_uart_tx_wait:
        la      r2,16711937
        lbu     r0,0(r2)
        lcu     r1,128
        and     r0,r1
        ceq     r0,z
        brf     _uart_tx_wait
        la      r2,16711936
        lw      r0,9(fp)
        sb      r0,0(r2)
        mov     sp,fp
        pop     r1
        pop     r2
        pop     fp
        jmp     (r1)

        .globl  _UART_PUTS
_UART_PUTS:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        lw      r2,9(fp)
_uart_puts_loop:
        lbu     r0,0(r2)
        ceq     r0,z
        brt     _uart_puts_done
        push    r2
        push    r0
        la      r0,_UART_PUTCHAR
        jal     r1,(r0)
        add     sp,3
        pop     r2
        lc      r0,1
        add     r2,r0
        bra     _uart_puts_loop
_uart_puts_done:
        lc      r0,10
        push    r0
        la      r0,_UART_PUTCHAR
        jal     r1,(r0)
        add     sp,3
        mov     sp,fp
        pop     r1
        pop     r2
        pop     fp
        jmp     (r1)

; --- ENTRY: main entry point ---
        .globl  _ENTRY
_ENTRY:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        ; set shared global _MODE to 1
        lc      r0,1
        la      r2,_MODE
        sw      r0,0(r2)
        ; print "main:enter"
        la      r0,_S_MAIN_ENTER
        push    r0
        la      r2,_UART_PUTS
        jal     r1,(r2)
        add     sp,3
        ; call _LIB_A
        la      r2,_LIB_A
        jal     r1,(r2)
        ; call _LIB_B
        la      r2,_LIB_B
        jal     r1,(r2)
        ; print "main:leave"
        la      r0,_S_MAIN_LEAVE
        push    r0
        la      r2,_UART_PUTS
        jal     r1,(r2)
        add     sp,3
        mov     sp,fp
        pop     r1
        pop     r2
        pop     fp
        jmp     (r1)

        .data
; --- shared globals (owned by main, referenced by all modules) ---
        .globl  _MODE
_MODE:
        .byte   0x00, 0x00, 0x00
        .globl  _COUNT
_COUNT:
        .byte   0x00, 0x00, 0x00

; --- local strings ---
_S_MAIN_ENTER:
        .byte   0x6D, 0x61, 0x69, 0x6E, 0x3A, 0x65, 0x6E, 0x74, 0x65, 0x72, 0x00
_S_MAIN_LEAVE:
        .byte   0x6D, 0x61, 0x69, 0x6E, 0x3A, 0x6C, 0x65, 0x61, 0x76, 0x65, 0x00
