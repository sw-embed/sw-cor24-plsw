# PL/SW Compiler -- Product Requirements Document

## Purpose

Implement a PL/SW (Programming Language for Software Wrighter) compiler
for the COR24 24-bit RISC ISA. PL/SW is a freestanding, PL/I-inspired
systems language with AST-aware compile-time metaprogramming and
controlled inline assembly, transpiling to human-readable COR24
assembler source (.s).

The compiler itself is written in C (compiled by tc24r) and runs
natively on COR24 hardware or emulator.

## Motivation

PL/SW is a conceptual homage to the IBM PL/S-family systems languages
(PL/S-III, PL/AS, PL/X) -- proprietary internal IBM languages used to
build OS/390, z/OS, and related system software. PL/SW recreates the
*spirit* of these languages without attempting compatibility:

- PL/I-flavored syntax instead of C syntax
- No mandatory runtime -- freestanding by design
- Macro-driven systems construction: high-level abstractions generating
  low-level boilerplate
- Controlled escape to inline assembly
- Transparent .s output for inspection and debugging

## Goals

1. **Working PL/SW compiler** that reads `.plsw` source and emits
   COR24 `.s` assembly
2. **PL/I-like syntax**: DCL, PROC, DO/END blocks, IF/THEN/ELSE,
   level-based structure declarations
3. **Integer-only types** matching COR24: BIT, BYTE, WORD, INT(n),
   CHAR, PTR, arrays, records, unions
4. **Macro preprocessor** processing `.msw` macro/declaration files
   with `?MACRO(...)` invocation syntax
5. **Inline assembly** via `ASM DO; ... END;` blocks
6. **Readable .s output** with source-line comments for debugging
7. **No float** -- COR24 has no FPU
8. **Self-contained** -- runs on COR24 emulator, no host dependencies

## Non-Goals (explicitly deferred)

- Floating-point arithmetic
- Full PL/I compatibility
- Sophisticated optimization (beyond basic register allocation)
- Separate compilation / linking (beyond symbol import/export)
- Exception handling / ON-units (v1 uses macro-based recovery)
- Decimal arithmetic
- Hygienic macro theory
- Multiple target ISAs (COR24 only for v1)

## Target Users

- The developer (exploring systems language design)
- Anyone building system software for COR24
- Educational use demonstrating compiler construction on minimal hardware

## Functional Requirements

### FR1: Source Language

- File extensions: `.plsw` (source), `.msw` (macro/declaration)
- PL/I-inspired declaration syntax with levels (1, 3, 5, ...)
- Procedures with explicit calling convention
- Control flow: IF/THEN/ELSE, DO WHILE, DO count, RETURN
- Expressions: arithmetic, comparison, logical, bitwise, shifts
- Pointer/address arithmetic
- Inline assembly blocks

### FR2: Macro System

- `.msw` files on configurable search path
- `?MACRO(args)` invocation with keyword arguments
- MACRODEF definitions with REQUIRED/OPTIONAL clauses
- GEN blocks for direct assembler generation within macros
- Phase 1: separate preprocessor pass (`.plsw` + `.msw` -> expanded `.plsw`)
- Phase 2: integrated macro processing in compiler

### FR3: Code Generation

- Emit COR24 assembly (.s) compatible with cor24-run assembler
- COR24 calling convention (stack args, r0 return, fp/sp management)
- Linear-scan register allocation with spilling
- Source-line annotations in generated .s for debugging

### FR4: Toolchain Integration

- Compiler invocation: `plsw <input.plsw> [-o output.s] [-I macrodir]`
- Pipeline: `.plsw` -> `plsw` compiler -> `.s` -> `cor24-run --run`
- Mixed listing output option (source + generated assembly)

## Platform Constraints

| Resource          | Specification                          |
|-------------------|----------------------------------------|
| Word size         | 24 bits                                |
| Registers         | 3 GP (r0-r2), fp, sp, z, iv, ir       |
| SRAM              | 1 MB (0x000000-0x0FFFFF)              |
| Stack (EBR)       | 8 KB (0xFEE000-0xFEFFFF)              |
| I/O               | UART at 0xFF0100, LED at 0xFF0000     |
| Arithmetic        | add, sub, mul (no hardware divide)     |
| Instruction sizes | 1, 2, or 4 bytes (variable-length)     |
