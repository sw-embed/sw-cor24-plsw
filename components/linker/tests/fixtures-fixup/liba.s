        .text
; liba -- library A, calls _UART_PUTS (from main) and _UTIL_INC (from util)
; References shared globals _MODE, _COUNT

        .globl  _LIB_A
_LIB_A:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        ; print "liba:enter"
        la      r0,_S_LIBA_ENTER
        push    r0
        la      r2,_UART_PUTS
        jal     r1,(r2)
        add     sp,3
        ; call _UTIL_INC to increment _COUNT
        la      r2,_UTIL_INC
        jal     r1,(r2)
        ; print "liba:leave"
        la      r0,_S_LIBA_LEAVE
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
_S_LIBA_ENTER:
        .byte   0x6C, 0x69, 0x62, 0x61, 0x3A, 0x65, 0x6E, 0x74, 0x65, 0x72, 0x00
_S_LIBA_LEAVE:
        .byte   0x6C, 0x69, 0x62, 0x61, 0x3A, 0x6C, 0x65, 0x61, 0x76, 0x65, 0x00
