# sw-cor24-plsw

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA.

A freestanding, PL/I-inspired systems language with compile-time macro
metaprogramming and controlled inline assembly, transpiling to
human-readable COR24 assembler source (.s).

## Status

Phase 7 complete — records and arrays. Record field access (dot notation
with computed offsets, nested records) and array element access (base +
index * element_size for INT/CHAR arrays, store/load, loop fill/readback).
Phases 0-6 complete: lexer, parser, AST, symbol table, type system,
storage layout, emitter framework, expression/assignment codegen, static
data, procedure prologue/epilogue, call codegen with R-to-L args,
recursion support, IF/THEN/ELSE, DO WHILE, DO count loops.

## Documentation

- [Product Requirements](docs/prd.md)
- [Architecture](docs/architecture.md)
- [Design Decisions](docs/design.md)
- [Implementation Plan](docs/plan.md)
- [Usage Guide](docs/usage.md)
- [Research Notes](docs/research.txt)

## Build

```bash
# Build
just build

# Run interactively
just run

# Run with UART input string
just run-input 'hello\n42\n'
```

## License

See [LICENSE](LICENSE).
