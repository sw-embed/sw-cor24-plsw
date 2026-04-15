        .text
; libb -- library B, calls _UART_PUTS (from main) and _UTIL_INC (from util)

        .globl  _LIB_B
_LIB_B:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        ; print "libb:enter"
        la      r0,_S_LIBB_ENTER
        push    r0
        la      r2,_UART_PUTS
        jal     r1,(r2)
        add     sp,3
        ; call _UTIL_INC
        la      r2,_UTIL_INC
        jal     r1,(r2)
        ; print "libb:leave"
        la      r0,_S_LIBB_LEAVE
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
_S_LIBB_ENTER:
        .byte   0x6C, 0x69, 0x62, 0x62, 0x3A, 0x65, 0x6E, 0x74, 0x65, 0x72, 0x00
_S_LIBB_LEAVE:
        .byte   0x6C, 0x69, 0x62, 0x62, 0x3A, 0x6C, 0x65, 0x61, 0x76, 0x65, 0x00
