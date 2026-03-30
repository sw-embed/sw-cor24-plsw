# PL/SW Compiler -- Design Decisions

## D1: PL/I-flavored syntax, not C

PL/SW uses PL/I-inspired syntax: `DCL`, `PROC`, `DO ... END`,
`IF ... THEN`, semicolon-terminated statements. This is deliberate:
PL/SW is a homage to the IBM PL/S-family systems languages, not
another C dialect.

Key syntax choices:
- `DCL` or `DECLARE` for declarations
- `PROC ... END` for procedures
- `DO; ... END;` for blocks
- `DO WHILE (cond); ... END;` for loops
- `IF (cond) THEN ... ELSE ...`
- `CALL proc(args)` for procedure calls
- `RETURN(expr)` for return values
- Level-based structure declarations (1, 3, 5, ...)

## D2: Level-based declarations

Structures use odd-numbered levels like PL/I, allowing future
union support without syntax changes:

```
DCL 1 SAVEAREA,
      3 BACKPTR   PTR,
      3 FORWPTR   PTR,
      3 REGS,
        5 R0      INT(24),
        5 R1      INT(24),
        5 R2      INT(24);
```

Even numbers are reserved for future extensions. Level numbers
need not be contiguous -- 1, 3, 7 is valid.

## D3: Integer-only type system

Matching COR24 hardware capabilities:
- `BIT` -- single bit (packed or boolean)
- `BYTE` -- 8-bit unsigned
- `WORD` -- 24-bit (native machine word)
- `INT(n)` -- signed integer, n = 8, 16, or 24
- `CHAR` -- 8-bit character
- `PTR` or `POINTER` -- 24-bit address
- Arrays: `type(count)` e.g. `CHAR(80)`
- Records: via level-based DCL
- Unions: via UNION attribute on level-1 DCL (future)

No floats, no decimal, no complex. This is a systems language
for a machine without an FPU.

## D4: Storage classes

- `STATIC` -- allocated at fixed address, lifetime = program
- `AUTOMATIC` -- allocated on stack, lifetime = scope (default)
- `EXTERNAL` -- declared here, defined elsewhere (linker symbol)
- `REGISTER` -- hint to prefer register allocation
- `BASED` -- accessed via explicit pointer (future)

Default is AUTOMATIC for locals, STATIC for globals.

## D5: Macro system -- two-phase approach

### Phase 1: Macro preprocessor (separate pass)
- `.plsw` + `.msw` -> expanded `.plsw`
- Built into the compiler binary but runs as a distinct phase
- Processes `%INCLUDE` directives to find `.msw` files
- Expands `?MACRO(...)` invocations
- Simple text/token-level expansion initially
- GEN blocks emit raw assembly strings

### Phase 2: AST-aware macros (later)
- Macros operate on syntax objects, not tokens
- MACRODEF with typed clause grammar
- Staged expansion (macros generating macros)
- Hygienic where practical

The preprocessor approach keeps v1 simple while proving the
macro invocation model. Phase 2 integrates macros into the
compiler for full AST awareness.

### Macro invocation syntax
```
?GETMAIN(LENGTH(256), SUBPOOL(0), ADDRESS(BUF), RC(RC));
```
- Leading `?` signals compile-time expansion
- Keyword arguments in `NAME(value)` form
- Semicolon terminated like all PL/SW statements

### Macro definition syntax (v1)
```
MACRODEF GETMAIN;
   REQUIRED LENGTH(expr);
   OPTIONAL SUBPOOL(expr);
   OPTIONAL ADDRESS(lvalue);
   OPTIONAL RC(lvalue);

   GEN DO;
      "mvi r0,{LENGTH}";
      "mvi r1,{SUBPOOL}";
      "svc 10";
   END;

   IF ADDRESS THEN
      "{ADDRESS} = r0";
   IF RC THEN
      "{RC} = r1";
END;
```

## D6: Inline assembly

Three levels of escape:

### Statement-level ASM block
```
ASM DO;
   "push r1";
   "mvi r1,42";
   "sw r1,0(r0)";
   "pop r1";
END;
```

### Naked procedure
```
PROC TIMER_ISR OPTIONS(NAKED);
   ASM DO;
      "push r0";
      ...
      "pop r0";
      "reti";
   END;
END;
```

### Expression-level (future)
```
X = ASM_EXPR("add %0,%1,%2", A, B);
```

ASM blocks are emitted verbatim into the .s output. The compiler
does not analyze register usage within ASM blocks -- the
programmer is responsible for correctness.

## D7: Code generation strategy

- **Template-based** initially: each AST pattern maps to a fixed
  instruction sequence
- **Virtual registers** during lowering, mapped to r0-r2 + spills
- **Linear-scan** register allocator (simple, predictable)
- **Spill to stack** when register pressure > 3
- **No optimization** beyond basic constant folding in v1
- **Source comments** in .s output for every source line

## D8: Compiler written in C for tc24r

The compiler itself is a C program compiled by tc24r. This means:
- Single translation unit (all .c files `#include`d or concatenated)
- No libc -- freestanding, UART I/O only
- Static allocation -- fixed-size tables, pools, buffers
- Must fit in COR24 memory constraints

## D9: Error handling

- Errors printed to UART with source line number
- First error halts compilation (no error recovery in v1)
- Error categories: SYNTAX, TYPE, STORAGE, UNDEFINED, INTERNAL

## D10: Output format

The compiler emits a single `.s` file containing:
- `.text` section with code
- `.data` section with static storage
- Labels for procedures and static variables
- Comments with source line references
- Standard COR24 assembly syntax (compatible with cor24-run)
