# sw-cor24-plsw

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA.

A freestanding, PL/I-inspired systems language with compile-time macro
metaprogramming and controlled inline assembly, transpiling to
human-readable COR24 assembler source (.s).

## Status

Phase 4 in progress — code generation. Assembly emitter framework
implemented with output buffer, section management (.text/.data),
unique label generation, instruction emission helpers, and COR24
prologue/epilogue generation. Phase 3 complete with scoped symbol table,
type system (width tracking, signedness, compatibility checking,
compound type descriptors), and storage layout computation (stack
offsets, static addresses, COR24 calling convention parameter offsets).

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
