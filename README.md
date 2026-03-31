# sw-cor24-plsw

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA.

A freestanding, PL/I-inspired systems language with compile-time macro
metaprogramming and controlled inline assembly, transpiling to
human-readable COR24 assembler source (.s).

## Status

Phase 3 complete — symbol table, type system, and storage layout. Scoped
symbol table with global and per-procedure scopes, shadowing,
case-insensitive lookup, and duplicate detection. Type system with width
tracking, signedness, assignment/expression compatibility checking,
pointer arithmetic rules, compound type descriptors for arrays and
records with computed field offsets. Storage layout computation assigns
stack offsets for AUTOMATIC locals (negative from fp), static addresses
for STATIC variables, parameter offsets per COR24 calling convention
(fp+9 base), and tracks per-procedure frame sizes.

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
