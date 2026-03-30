# PL/SW Compiler -- Usage Guide

## Building the Compiler

The PL/SW compiler is written in C and compiled by tc24r to run
on the COR24 emulator.

```bash
# Compile the PL/SW compiler itself
tc24r src/plsw.c -o build/plsw.s

# Run the compiler on the emulator to compile a PL/SW program
# (the compiler reads .plsw from UART/stdin and emits .s to UART/stdout)
cor24-run --run build/plsw.s < examples/hello.plsw > output.s

# Assemble and run the compiled program
cor24-run --run output.s
```

## Compiler Invocation

When running on COR24, the compiler reads from UART input and writes
assembly to UART output. Include paths for .msw files are configured
at compile time or via a simple command-line protocol.

## Source File Types

| Extension | Purpose                                          |
|-----------|--------------------------------------------------|
| `.plsw`   | PL/SW source code                                |
| `.msw`    | Macro definitions and shared declarations        |
| `.s`      | Generated COR24 assembly (compiler output)       |

## Language Quick Reference

### Declarations

```plsw
/* Simple scalar declarations */
DCL COUNT INT(24);
DCL FLAGS BYTE;
DCL LETTER CHAR;
DCL BUFPTR PTR;

/* Array */
DCL BUFFER(80) CHAR;

/* Record with levels */
DCL 1 POINT,
      3 X INT(24),
      3 Y INT(24);

/* Storage classes */
DCL COUNTER INT(24) STATIC;
DCL TEMP INT(24) AUTOMATIC;      /* default for locals */
DCL EXTERN_SYM INT(24) EXTERNAL;
```

### Procedures

```plsw
PROC ADD(A INT(24), B INT(24)) RETURNS(INT(24));
   RETURN(A + B);
END;

PROC MAIN OPTIONS(FREESTANDING);
   DCL RESULT INT(24);
   RESULT = CALL ADD(3, 4);
   CALL PRINT_INT(RESULT);
END;
```

### Control Flow

```plsw
/* IF/THEN/ELSE */
IF (COUNT > 0) THEN DO;
   CALL PROCESS();
END;
ELSE DO;
   CALL INIT();
END;

/* DO WHILE */
DO WHILE (COUNT > 0);
   CALL STEP(COUNT);
   COUNT = COUNT - 1;
END;

/* Counted DO */
DO I = 1 TO 10;
   CALL PRINT_INT(I);
END;
```

### Inline Assembly

```plsw
/* Statement-level ASM block */
ASM DO;
   "push r1";
   "mvi r1,42";
   "sw r1,0xFF0000(z)";   /* write to LED */
   "pop r1";
END;

/* Naked procedure (no prologue/epilogue) */
PROC IRQ_HANDLER OPTIONS(NAKED);
   ASM DO;
      "push r0";
      "push r1";
      /* ... handler logic ... */
      "pop r1";
      "pop r0";
      "reti";
   END;
END;
```

### Macro Invocation

```plsw
/* Include macro definitions */
%INCLUDE SYSTEM;    /* finds SYSTEM.msw on search path */

/* Invoke a macro */
?GETMAIN(LENGTH(256), SUBPOOL(0), ADDRESS(BUF));

/* Macro with optional clauses */
?ENTRY(SAVE(R1,R2), WORKAREA(MYWS));
```

### Expressions

```plsw
/* Arithmetic */
X = A + B * C - D;

/* Comparison */
IF (X >= 10) THEN ...

/* Bitwise */
FLAGS = FLAGS & MASK;
FLAGS = FLAGS | BIT3;
SHIFTED = X << 4;

/* Pointer arithmetic */
NEXT = PTR + 3;    /* advance 3 bytes */
```

## Example: Hello World

```plsw
/* hello.plsw -- Hello World for COR24 */

DCL MSG(18) CHAR STATIC INIT('Hello from PL/SW!');

PROC UART_PUTC(CH CHAR);
   ASM DO;
      "lw r0,9(fp)";          /* load char argument */
      "sw r0,0xFF0100(z)";    /* write to UART data */
   END;
END;

PROC UART_PUTS(S PTR);
   DCL I INT(24);
   DCL CH CHAR;
   I = 0;
   DO WHILE (1);
      CH = S(I);              /* S treated as char array via ptr */
      IF (CH = 0) THEN RETURN;
      CALL UART_PUTC(CH);
      I = I + 1;
   END;
END;

PROC MAIN OPTIONS(FREESTANDING);
   CALL UART_PUTS(ADDR(MSG));
END;
```

## Build Pipeline

```
hello.plsw + system.msw
       |
       v
   PL/SW Compiler (on COR24)
       |
       v
   hello.s (COR24 assembly)
       |
       v
   cor24-run --run hello.s
       |
       v
   "Hello from PL/SW!" on UART
```

## Macro Definition Example

```msw
/* system.msw -- System service macros for COR24 */

MACRODEF GETMAIN;
   REQUIRED LENGTH(expr);
   OPTIONAL SUBPOOL(expr);
   OPTIONAL ADDRESS(lvalue);
   OPTIONAL RC(lvalue);

   GEN DO;
      "mvi r0,{LENGTH}";
      "svc 10";
   END;

   IF ADDRESS THEN DO;
      ADDRESS = ASM_EXPR("mov %0,r0");
   END;
   IF RC THEN DO;
      RC = ASM_EXPR("mov %0,r1");
   END;
END;
```
