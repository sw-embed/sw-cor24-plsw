# PL/SW Compiler -- Architecture

## Overview

The PL/SW compiler is a source-to-assembly transpiler targeting the
COR24 24-bit RISC ISA. Written in C (compiled by tc24r), it runs
natively on COR24 hardware or emulator. Input is `.plsw` source files
plus `.msw` macro/declaration files; output is a single `.s` assembly
file ready for the COR24 assembler.

## System Context

```
Developer
  |
  v
.plsw source + .msw macros
  |
  v
+----------------------------------+
| PL/SW Compiler (C via tc24r)     |
|  +----------------------------+  |
|  | Macro Preprocessor         |  |
|  | Lexer / Tokenizer          |  |
|  | Parser (recursive descent) |  |
|  | AST Construction           |  |
|  | Semantic Analysis          |  |
|  | IR Lowering                |  |
|  | Register Allocator         |  |
|  | Assembly Emitter           |  |
|  +----------------------------+  |
+----------------------------------+
  |
  v
output.s (COR24 assembly)
  |
  v
cor24-run --run output.s
  |
  v
COR24 CPU (emulator or FPGA)
```

## Compilation Pipeline

```
.plsw + .msw
  |
  v
[1] Macro Preprocessing
    - Parse .msw macro definitions
    - Resolve include paths (-I dirs)
    - Expand ?MACRO(...) invocations
    - Process GEN blocks into inline asm
    - Output: expanded source stream
  |
  v
[2] Lexical Analysis
    - Tokenize expanded source
    - Token types: keywords, identifiers, literals, operators, punctuation
    - Handle inline ASM blocks as opaque token sequences
  |
  v
[3] Parsing
    - Recursive descent, top-down
    - Build AST nodes from pre-allocated pool
    - DCL statements -> type/storage nodes
    - PROC -> procedure nodes with parameter lists
    - Expressions -> expression trees
    - Inline ASM -> opaque ASM nodes
  |
  v
[4] Semantic Analysis
    - Symbol table: scoped (global + per-procedure)
    - Type checking (integer widths, pointer compatibility)
    - Storage class validation (STATIC, AUTOMATIC, EXTERNAL)
    - Record layout computation (offsets, sizes, alignment)
    - Resolve EXTERNAL references
  |
  v
[5] IR Lowering
    - Desugar control flow (DO WHILE -> labels + branches)
    - Lower record field access to offset arithmetic
    - Lower array indexing to address calculation
    - Expand pointer dereference
    - Flatten nested expressions to virtual-register SSA-like form
  |
  v
[6] Register Allocation
    - Linear-scan allocation over virtual registers
    - Map to r0, r1, r2 (3 GP registers)
    - Spill to stack when pressure exceeds 3
    - Reserve fp, sp for frame management
  |
  v
[7] Assembly Emission
    - Emit COR24 assembly with labels, directives
    - Procedure prologue/epilogue (push fp, save regs, set fp)
    - Source-line comments for debugging
    - Inline ASM blocks emitted verbatim
    - Static data in .data section
```

## Memory Layout (COR24)

```
0x000000 +------------------+
         | Code (.text)     |
         |                  |
         +------------------+
         | Static Data      |
         | (.data section)  |
         +------------------+
         | Heap             |
         | (grows upward)   |
         |                  |
         +------------------+
         |                  |
         | (free space)     |
         |                  |
0x0FFFFF +------------------+

0xFEE000 +------------------+
         | Stack (EBR)      |
         | (grows downward) |
0xFEFFFF +------------------+

0xFF0000 +------------------+
         | LED I/O          |
0xFF0100 +------------------+
         | UART I/O         |
0xFF0101 +------------------+
         | UART Status      |
         +------------------+
```

## Key Data Structures

### Token
```c
typedef struct {
    int type;       /* TOK_DCL, TOK_PROC, TOK_IDENT, TOK_NUM, ... */
    int value;      /* numeric value or symbol table index */
    char name[32];  /* identifier text */
} Token;
```

### AST Node
```c
typedef struct ASTNode {
    int kind;           /* NODE_DCL, NODE_PROC, NODE_IF, NODE_EXPR, ... */
    int type_info;      /* type tag for semantic analysis */
    int storage_class;  /* STATIC, AUTOMATIC, EXTERNAL */
    int level;          /* declaration level (1, 3, 5, ...) */
    struct ASTNode *left, *right, *next;
    char name[32];
    int int_val;
} ASTNode;
```

### Symbol Table Entry
```c
typedef struct {
    char name[32];
    int type;           /* TYPE_INT, TYPE_PTR, TYPE_CHAR, TYPE_RECORD */
    int width;          /* bit width: 8, 16, 24 */
    int storage;        /* STOR_STATIC, STOR_AUTO, STOR_EXTERNAL */
    int offset;         /* stack offset or static address */
    int level;          /* declaration level */
    int scope;          /* scope depth */
} Symbol;
```

## COR24 Calling Convention

- Arguments pushed right-to-left on stack (3 bytes each)
- Return value in r0
- Caller saves r0 if needed across calls
- Callee saves r1, r2, fp
- Prologue: push fp, push r2, push r1, mov fp,sp
- Epilogue: mov sp,fp, pop r1, pop r2, pop fp, ret
- First arg at fp+9 (3 saved regs x 3 bytes)

## Source File Types

| Extension | Purpose                                      |
|-----------|----------------------------------------------|
| `.plsw`   | PL/SW source code                            |
| `.msw`    | Macro definitions, shared declarations       |
| `.s`      | Generated COR24 assembly output              |

## Compiler Limitations (v1)

- Single translation unit (no separate compilation)
- 3 GP registers require aggressive spilling
- No floating point
- Static allocation preferred; heap via OS service macros
- Max ~64 symbols per scope (linear scan)
- Max ~256 AST nodes (pre-allocated pool)
