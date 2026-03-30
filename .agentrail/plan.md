# PL/SW Compiler -- Implementation Plan

## Phase Overview

| Phase | Description                     | Deliverable                              |
|-------|---------------------------------|------------------------------------------|
| 0     | Project setup and toolchain     | Build system, hello-world on emulator    |
| 1     | Lexer and token infrastructure  | Tokenize PL/SW source                   |
| 2     | Parser and AST                  | Parse core language constructs           |
| 3     | Symbol table and type system    | Scoped symbols, type checking            |
| 4     | Code generation basics          | Emit .s for simple programs              |
| 5     | Procedures and calling conv.    | PROC, CALL, RETURN with stack frames     |
| 6     | Control flow                    | IF/THEN/ELSE, DO WHILE, DO count        |
| 7     | Records and arrays              | Level-based structures, array access     |
| 8     | Pointers and addresses          | PTR type, address-of, dereference        |
| 9     | Inline assembly                 | ASM DO blocks, naked procedures          |
| 10    | Macro preprocessor              | .msw parsing, ?MACRO expansion, GEN      |
| 11    | Demo programs and testing       | UART hello, LED toggle, service wrappers |
| 12    | Polish and documentation        | Listings, error messages, docs           |

---

## Phase 0: Project Setup and Toolchain Validation

### Step 0.1: Repository structure
- Create `src/` for compiler C source
- Create `test/` for test PL/SW programs
- Create `examples/` for demo programs
- Create build script for tc24r + cor24-run pipeline
- Verify tc24r compiles a trivial C program
- Verify cor24-run assembles and executes it

### Step 0.2: UART I/O bootstrap
- Write uart_putchar, uart_getchar, uart_puts, uart_getline
- Write print_int for decimal output
- Test: compiler shell prints banner and reads input

### Step 0.3: String and memory utilities
- Write strcmp, strcpy, strlen, memset, memcpy
- Write simple arena allocator for compiler tables
- Test: basic string operations work

---

## Phase 1: Lexer and Token Infrastructure

### Step 1.1: Token types and keyword table
- Define token types: DCL, PROC, IF, THEN, ELSE, DO, WHILE, END,
  CALL, RETURN, ASM, IDENT, NUM, operators, punctuation
- Keyword lookup table (DCL, DECLARE, PROC, IF, THEN, ...)
- Test: identify keywords vs identifiers

### Step 1.2: Lexer implementation
- Scan input buffer character by character
- Handle: identifiers, numbers, string literals, operators
- Skip whitespace and comments (/* ... */ style)
- Handle multi-character operators (<=, >=, !=, ->)
- Test: tokenize sample PL/SW fragments

---

## Phase 2: Parser and AST

### Step 2.1: AST node pool and types
- Pre-allocated node pool (256 nodes)
- Node kinds: PROGRAM, DCL, PROC, IF, DO_WHILE, ASSIGN,
  BINOP, UNOP, CALL, RETURN, LITERAL, IDENT, ASM_BLOCK
- Allocator and tree-building helpers

### Step 2.2: Declaration parser
- Parse DCL statements with type attributes
- Parse level-based structure declarations
- Parse storage class attributes (STATIC, AUTOMATIC, EXTERNAL)
- Test: parse various DCL forms

### Step 2.3: Expression parser
- Binary operators: +, -, *, arithmetic
- Comparison operators: =, <, >, <=, >=, !=
- Logical operators: &, |, ^, ~ (bitwise)
- Shift operators: << >> (or SHL, SHR keywords)
- Parenthesized sub-expressions
- Operator precedence (standard arithmetic precedence)
- Test: parse expression trees

### Step 2.4: Statement parser
- Assignment: `var = expr;`
- IF/THEN/ELSE
- DO WHILE, DO count
- CALL proc(args)
- RETURN(expr)
- DO; ... END; blocks
- Test: parse compound statements

### Step 2.5: Procedure parser
- PROC name OPTIONS(...); ... END;
- Parameter declarations
- Local declarations
- RETURNS(type) clause
- Test: parse complete procedures

### Step 2.6: Top-level parser
- Parse sequence of top-level DCL and PROC
- Build program AST
- Test: parse multi-procedure programs

---

## Phase 3: Symbol Table and Type System

### Step 3.1: Symbol table
- Scoped symbol table (global + per-procedure)
- Insert, lookup, scope enter/exit
- Track: name, type, storage class, offset, level
- Test: nested scopes resolve correctly

### Step 3.2: Type representation
- Internal type tags: INT8, INT16, INT24, BYTE, CHAR, PTR,
  ARRAY, RECORD
- Width and signedness attributes
- Record field lists with offsets
- Test: type construction and comparison

### Step 3.3: Type checking
- Expression type inference and checking
- Assignment compatibility
- Procedure argument/return type checking
- Pointer type validation
- Test: type errors detected correctly

### Step 3.4: Storage layout
- Compute record sizes and field offsets from level declarations
- Compute array sizes
- Assign stack offsets for AUTOMATIC variables
- Assign static addresses for STATIC variables
- Test: layout matches expected sizes

---

## Phase 4: Code Generation Basics

### Step 4.1: Assembly emitter framework
- Output buffer and emit functions
- Section management (.text, .data)
- Label generation (unique labels for branches)
- Comment emission (source line tracking)
- Test: emit skeleton .s file

### Step 4.2: Expression codegen
- Literal loading (mvi)
- Variable load/store (lw/sw with offsets)
- Binary arithmetic (add, sub, mul)
- Software divide routine
- Comparison to branch conditions
- Register allocation for expression temporaries
- Test: evaluate expressions, print results via UART

### Step 4.3: Assignment codegen
- Simple variable assignment
- Record field assignment (base + offset)
- Array element assignment (base + index * size)
- Test: assign and read back values

### Step 4.4: Static data emission
- String literals in .data section
- Static variable allocation
- Initial values
- Test: static data accessible at runtime

---

## Phase 5: Procedures and Calling Convention

### Step 5.1: Procedure prologue/epilogue
- Push fp, save callee-saved registers
- Set up frame pointer
- Allocate stack space for locals
- Epilogue: restore and return
- Test: empty procedure call/return works

### Step 5.2: Procedure calls
- Evaluate arguments, push right-to-left
- CALL instruction
- Clean up stack after return
- Capture return value from r0
- Test: call with arguments, get return value

### Step 5.3: Nested calls and recursion
- Correct frame management for nested calls
- Register save/restore across calls
- Test: recursive factorial

---

## Phase 6: Control Flow

### Step 6.1: IF/THEN/ELSE
- Evaluate condition
- Conditional branch to else/end label
- Test: if/then, if/then/else

### Step 6.2: DO WHILE loop
- Loop header label
- Condition evaluation and branch
- Loop body
- Branch back to header
- Test: counted loop via while

### Step 6.3: DO count loop (iteration)
- Loop variable, start, end, optional increment
- Lowered to while-equivalent
- Test: DO I = 1 TO 10

---

## Phase 7: Records and Arrays

### Step 7.1: Record field access
- Dot notation: `rec.field`
- Computed offset from record layout
- Nested record access: `rec.sub.field`
- Test: declare, assign, read record fields

### Step 7.2: Array access
- Index expression: `arr(i)`
- Bounds checking (optional, controlled by option)
- Multi-dimensional arrays (future)
- Test: declare, fill, read array

---

## Phase 8: Pointers and Addresses

### Step 8.1: Pointer operations
- ADDR(var) -- address-of operator
- PTR-> dereference
- Pointer arithmetic (add offset)
- Test: pointer to record, pointer to array element

### Step 8.2: BASED storage (future)
- Variables accessed via explicit base pointer
- Overlay semantics
- Test: control block access via pointer

---

## Phase 9: Inline Assembly

### Step 9.1: ASM DO blocks
- Parse ASM DO; "string"; ... END;
- Emit assembly strings verbatim into .s output
- String interpolation for variable references
- Test: inline asm that manipulates registers

### Step 9.2: Naked procedures
- PROC with OPTIONS(NAKED) skips prologue/epilogue
- Entire body is ASM
- Test: interrupt handler stub

---

## Phase 10: Macro Preprocessor

### Step 10.1: Include processing
- %INCLUDE directive
- Search path from -I flags
- Read and insert .msw file contents
- Test: include a .msw file

### Step 10.2: Macro definition parsing
- Parse MACRODEF ... END; in .msw files
- Store macro name, clause grammar, body
- REQUIRED and OPTIONAL clauses
- Test: parse a macro definition

### Step 10.3: Macro expansion
- Recognize ?MACRO(...) in source
- Match keyword arguments to clauses
- Substitute arguments into macro body
- Expand to PL/SW source tokens
- Test: ?GETMAIN expands correctly

### Step 10.4: GEN blocks
- GEN DO; ... END; within macro bodies
- Emit assembly directly during expansion
- Template substitution for arguments
- Test: macro with GEN emits correct assembly

### Step 10.5: Conditional compilation
- %IF / %THEN / %ELSE / %ENDIF
- Based on defined symbols or target flags
- Test: conditional include/exclude

---

## Phase 11: Demo Programs and Testing

### Step 11.1: UART hello world
- PL/SW program that prints "Hello from PL/SW!" via UART
- End-to-end: .plsw -> compiler -> .s -> cor24-run

### Step 11.2: LED toggle
- MMIO write to LED address using inline ASM
- Test on emulator

### Step 11.3: Counted loop with output
- Loop printing numbers 1..10
- Demonstrates DO, CALL, arithmetic

### Step 11.4: Record and pointer demo
- Declare a record, fill fields, access via pointer
- Demonstrates level-based DCL, PTR, field access

### Step 11.5: Macro demo
- .msw file with service macro
- .plsw file invoking macro
- Demonstrates macro pipeline

---

## Phase 12: Polish and Documentation

### Step 12.1: Mixed listing output
- Source line + generated assembly interleaved
- Controlled by compiler flag

### Step 12.2: Error messages
- Line numbers in all error messages
- Descriptive error categories

### Step 12.3: Final documentation
- Update README with usage, examples
- Document language syntax reference
- Document macro system reference
- Example session transcripts
