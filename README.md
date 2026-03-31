# sw-cor24-plsw

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA.

A freestanding, PL/I-inspired systems language with compile-time macro
metaprogramming and controlled inline assembly, transpiling to
human-readable COR24 assembler source (.s).

## Status

Phase 4 in progress — code generation. Expression codegen implemented
with literal loading (lc/la), variable load/store (fp-relative and
static), binary arithmetic (add, sub, mul), software division via
__plsw_div subroutine, all comparison operators (ceq/cls with carry
flag materialization), unary negate, register allocation with spill to
stack when pressure exceeds 3 registers, and assignment codegen.
Assembly emitter framework provides output buffer, section management,
label generation, instruction emission, and COR24 prologue/epilogue.
Phase 3 complete with scoped symbol table, type system, and storage
layout computation.

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
