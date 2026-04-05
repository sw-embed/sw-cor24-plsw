# sw-cor24-plsw

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA.

A freestanding, PL/I-inspired systems language with compile-time macro
metaprogramming, BASED record templates, and controlled inline assembly.
Transpiles to human-readable COR24 assembler source (.s) with
interleaved source-line comments.

## Status: v1 Complete

All 40 implementation steps complete across 12 phases:

- **Lexer** -- tokenizer with keywords, operators, string/number literals
- **Parser** -- recursive descent for DCL, PROC, IF, DO WHILE, DO count
- **AST** -- 256-node pool with 18 node kinds
- **Symbol table** -- scoped (global + per-procedure), type tracking
- **Type system** -- INT(8/16/24), BYTE, CHAR, PTR, WORD, BIT, RECORD
- **Storage layout** -- stack frames, static data, record field offsets
- **Code generation** -- COR24 assembly for all constructs
- **Inline assembly** -- ASM DO blocks, NAKED procedures
- **Macros** -- MACRODEF/GEN, %INCLUDE, ?MACRO() invocation
- **Conditionals** -- %IF/%DEFINE/%ELSE/%ENDIF
- **Built-ins** -- ADDR(), SIZEOF(), string literal assignment to ptr->field
- **Listing** -- source lines as assembly comments, line numbers in errors
- **Pipeline** -- FILE: protocol for multi-file compilation via UART

## Quick Start

```bash
# Build the compiler (requires tc24r and cor24-run)
just build

# Compile and run hello world
just pipeline examples/hello.plsw
# Output: Hello from PL/SW!

# Compile with .msw includes and memory dump
just chain
# Output: build/chain.s, build/chain-dump.txt, build/chain-combined.plsw

# Compile macro demo
just hello-macro
# Output: Hello from macros!
```

## Examples

| File | Description |
|------|-------------|
| `examples/hello.plsw` | Hello world via UART_PUTS |
| `examples/led.plsw` | LED MMIO toggle with inline ASM |
| `examples/loop.plsw` | DO I = 1 TO 10 with PRINT_INT |
| `examples/record.plsw` | Record fields and pointer dereference |
| `examples/chain.plsw` | Control block chain (CVT/ASCB/ASXB/TCB) with arena allocator |
| `examples/macro.plsw` | ?LED_SET and ?NOP macro invocation |
| `examples/hello_macro.plsw` | ?GREET macro via %INCLUDE |

## Pipeline

The compiler runs on the COR24 emulator. Source is fed via UART:

```
.plsw + .msw files  -->  pipeline.sh  -->  compiler on emulator
                                               |
                                           .s assembly
                                               |
                                          cor24-run --run  -->  program output
```

For multi-file compilation, the FILE:/SOURCE: protocol registers
named include files so `%INCLUDE name;` resolves them:

```bash
./scripts/pipeline-dump.sh include/cvt.msw include/ascb.msw examples/chain.plsw
```

Output files: `build/<name>.s`, `build/<name>-dump.txt`, `build/<name>-combined.plsw`

## Build System

```bash
just build          # Compile the PL/SW compiler itself
just run            # Interactive mode (terminal)
just test           # Run with instruction limit
just pipeline <f>   # Compile and run a .plsw
just pipeline-dump <files>  # Compile, run with memory dump
just chain          # Control block chain demo
just hello-macro    # Macro demo
just clean          # Remove build artifacts
```

## Documentation

- [Usage Guide & Language Reference](docs/usage.md)
- [Product Requirements](docs/prd.md)
- [Architecture](docs/architecture.md)
- [Design Decisions](docs/design.md)
- [Implementation Plan](docs/plan.md)
- [Research Notes](docs/research.txt)
- [Assembler Directive Report](docs/byte-comm.md)

## COR24 Target

- 24-bit RISC, 8 registers (r0-r2 GP, fp, sp, z, iv, ir)
- 1 MB SRAM + 8 KB EBR stack
- UART at 0xFF0100 (data), 0xFF0101 (status)
- LED at 0xFF0000 (active-low)
- Variable-length instructions (1, 2, or 4 bytes)
- Calling convention: args on stack R-to-L, return in r0

## License

See [LICENSE](LICENSE).
