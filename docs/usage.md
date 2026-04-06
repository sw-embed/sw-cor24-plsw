# PL/SW Compiler -- Usage Guide & Language Reference

## Building the Compiler

The PL/SW compiler is written in C and compiled by tc24r to run
on the COR24 emulator.

```bash
just build    # tc24r src/main.c -o build/plsw.s
```

## Compiling PL/SW Programs

Use the pipeline scripts to compile and run .plsw programs:

```bash
# Simple program (no includes)
just pipeline examples/hello.plsw

# Program with .msw include files (FILE: protocol)
just pipeline-dump include/cvt.msw include/ascb.msw examples/chain.plsw

# Shortcut targets for demos
just chain          # control block chain with .msw includes
just hello-macro    # macro invocation demo
```

### Pipeline Output

`pipeline-dump` produces three files in `build/`:

| File | Contents |
|------|----------|
| `build/<name>.s` | Generated assembly with source line comments |
| `build/<name>-dump.txt` | Emulator memory dump (registers, SRAM, I/O) |
| `build/<name>-combined.plsw` | All .msw + .plsw concatenated with path comments |

## Source File Types

| Extension | Purpose |
|-----------|---------|
| `.plsw` | PL/SW source code |
| `.msw` | Macro definitions and shared declarations |
| `.s` | Generated COR24 assembly (compiler output) |

## Compiler Modes

The compiler runs on the COR24 emulator with UART I/O. At startup
it prompts for a mode:

- **Suite number** (0-34) -- run a specific test suite
- **`a`** -- run all test suites
- **`c`** -- compile mode (accepts source via UART)
- **`r`** -- REPL (tokenizer)

### Compile Mode Protocol

In compile mode, the compiler accepts two input formats:

**Legacy** (single file): raw PL/SW source terminated by EOT (0x04).

**FILE:/SOURCE:** (multi-file): named include files followed by main source:
```
FILE:cvt.msw\n<content>\x1E
FILE:ascb.msw\n<content>\x1E
SOURCE:\n<main.plsw source>\x04
```

Each FILE: block registers content via `inc_register()` for `%INCLUDE` resolution.

## Language Reference

### Types

| Type | Width | Description |
|------|-------|-------------|
| `INT` or `INT(24)` | 3 bytes | Default 24-bit integer |
| `INT(16)` | 2 bytes | 16-bit integer |
| `INT(8)` | 1 byte | 8-bit integer |
| `BYTE` | 1 byte | Unsigned byte |
| `CHAR` | 1 byte | Character |
| `PTR` | 3 bytes | 24-bit pointer |
| `WORD` | 3 bytes | 24-bit unsigned |
| `BIT` | 1 byte | Boolean |

### Declarations

```plsw
/* Scalar */
DCL COUNT INT;
DCL FLAGS BYTE;
DCL LETTER CHAR;

/* With explicit width */
DCL ASID INT(16);

/* Array */
DCL BUFFER(80) CHAR;
DCL TABLE(10) INT;

/* String initialized */
DCL MSG(20) CHAR INIT('Hello from PL/SW!');

/* Numeric initialized */
DCL ARENA_POS INT INIT(0);

/* Record with levels (1, 3, 5, ...) */
DCL 1 POINT,
    3 X INT,
    3 Y INT;

/* BASED record (template for pointer access) */
DCL 1 CVT BASED,
    3 CVTEYE(4) CHAR,
    3 CVTASCBH  PTR;

/* Pointer associated with preceding BASED record */
DCL CVTPTR PTR;

/* Storage classes */
DCL COUNTER INT STATIC;
DCL TEMP INT AUTOMATIC;     /* default for locals */
DCL SYM INT EXTERNAL;
```

### Procedures

```plsw
/* Simple procedure */
MAIN: PROC;
    CALL UART_PUTS(ADDR(MSG));
END;

/* With parameters and return type */
ADD: PROC(A INT, B INT) RETURNS(INT);
    RETURN(A + B);
END;

/* Call as expression (without CALL keyword) */
RESULT = ADD(3, 4);

/* Call as statement */
CALL UART_PUTS(ADDR(MSG));

/* Naked procedure (no prologue/epilogue) */
ISR: PROC OPTIONS(NAKED);
    ASM DO;
        'push    r0';
        'pop     r0';
        'jmp     (iv)';
    END;
END;
```

### Control Flow

```plsw
/* IF/THEN */
IF (COUNT > 0) THEN
    CALL PROCESS();

/* IF/THEN/ELSE with blocks */
IF (X = 0) THEN DO;
    X = 1;
END;
ELSE DO;
    X = 0;
END;

/* DO WHILE */
DO WHILE (COUNT > 0);
    COUNT = COUNT - 1;
END;

/* Counted DO */
DO I = 1 TO 10;
    CALL PRINT_INT(I);
END;
```

### Expressions

```plsw
/* Arithmetic: +, -, *, / */
X = A + B * C;
D = N / 10;

/* Comparison: =, <, >, <=, >=, != */
IF (X >= 10) THEN ...

/* Bitwise: &, |, ^, ~ */
FLAGS = FLAGS & MASK;

/* Pointer arithmetic */
NEXT = PTR + 3;

/* Address-of */
P = ADDR(VARIABLE);
P = ADDR(RECORD);

/* Size-of (compile-time record size) */
SZ = SIZEOF(CVT);
P = ALLOC(SIZEOF(ASCB));
```

### Record and Pointer Access

```plsw
/* Direct field access */
POINT.X = 100;
POINT.Y = 200;

/* Pointer dereference */
CVTPTR->CVTEYE = 'CVT ';    /* string literal to CHAR array field */
CVTPTR->CVTASCBH = ASCBPTR;  /* pointer field */

/* Array access */
BUFFER(0) = 65;
DIGITS(I) = N + 48;
```

### Inline Assembly

```plsw
/* ASM DO block -- strings are emitted verbatim as instructions */
ASM DO;
    'la      r2,16711936';     /* UART data register */
    'lw      r0,9(fp)';       /* load parameter */
    'sb      r0,0(r2)';       /* store byte */
END;
```

Note: ASM strings use **single quotes**, not double quotes.

### Preprocessor Directives

```plsw
/* Include a .msw file */
%INCLUDE system;          /* finds system.msw via include registry */

/* Conditional compilation */
%DEFINE TARGET_COR24;
%IF DEFINED(TARGET_COR24);
    DCL UART_ADDR INT INIT(16711936);
%ENDIF;

/* Compile-time constants (value substitution) */
%DEFINE BUFSIZE 256;          /* numeric -- substituted as integer literal */
%DEFINE NEWLINE 10;
DCL BUF(BUFSIZE) BYTE;       /* BUFSIZE -> 256 at lex time, zero runtime cost */
CALL UART_PUTCHAR(NEWLINE);   /* NEWLINE -> 10 */
```

### Macro System

Macro definitions in .msw files:

```msw
MACRODEF LED_SET;
    REQUIRED VAL(expr);
    GEN DO;
        'la      r2,16711680';
        'lc      r0,{VAL}';
        'sb      r0,0(r2)';
    END;
END;
```

Macro invocation in .plsw source:

```plsw
%INCLUDE system;
?LED_SET(VAL(1));      /* expands to ASM DO block */
```

GEN blocks expand to `ASM DO; ... END;` with `{KEYWORD}` substitution.

Note: macro clause keywords must not collide with PL/SW reserved words
(e.g., use `CH` not `CHAR`, `VAL` not `INT`).

## Example: Hello World

```plsw
/* hello.plsw */
DCL MSG(20) CHAR INIT('Hello from PL/SW!');

MAIN: PROC;
    CALL UART_PUTS(ADDR(MSG));
END;
```

```
$ just pipeline examples/hello.plsw
=== Compiled hello.plsw (87 lines of assembly) ===
=== Running ===
Hello from PL/SW!
```

## Example: Control Block Chain

```plsw
/* chain.plsw -- uses .msw includes for record templates */
%INCLUDE cvt;
DCL CVTPTR PTR;
%INCLUDE ascb;
DCL ASCBPTR PTR;

MAIN: PROC;
    CVTPTR = ALLOC(SIZEOF(CVT));
    CVTPTR->CVTEYE = 'CVT ';
    CVTPTR->CVTASCBH = ASCBPTR;
    ...
END;
```

```
$ just chain
=== Compiling chain.plsw ===
  registered: cvt.msw
  registered: ascb.msw
  ...
=== Running with --dump ===
  Instructions: 317
  Halted: true
```

Memory dump shows eyecatchers: `CVT `, `ASCB`, `ASXB`, `TCB `.

## Generated Assembly

The compiler emits COR24 assembly with source line comments:

```asm
; 28:     CVTPTR  = ALLOC(SIZEOF(CVT));
        lc      r0,15
        push    r0
        la      r2,_ALLOC
        jal     r1,(r2)
        add     sp,3
        la      r2,_CVTPTR
        sw      r0,0(r2)
```

Runtime stubs for UART_PUTCHAR (with TX busy-wait) and UART_PUTS
are emitted automatically.

## Error Messages

Errors include source line numbers:

```
SYNTAX ERROR line 3: expected expression
SYNTAX ERROR line 5: unexpected token
  expected ;, got END
CODEGEN ERROR: undefined variable for store
STORAGE ERROR: ...
```
