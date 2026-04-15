        .text
; util -- utility library, references shared global _COUNT from main
; Also calls _UART_PUTS from main

        .globl  _UTIL_INC
_UTIL_INC:
        push    fp
        push    r2
        push    r1
        mov     fp,sp
        ; print "util:inc"
        la      r0,_S_UTIL_INC
        push    r0
        la      r2,_UART_PUTS
        jal     r1,(r2)
        add     sp,3
        ; increment shared global _COUNT
        la      r2,_COUNT
        lw      r0,0(r2)
        lc      r1,1
        add     r0,r1
        sw      r0,0(r2)
        mov     sp,fp
        pop     r1
        pop     r2
        pop     fp
        jmp     (r1)

        .data
_S_UTIL_INC:
        .byte   0x75, 0x74, 0x69, 0x6C, 0x3A, 0x69, 0x6E, 0x63, 0x00
