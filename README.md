# sw-cor24-plsw

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA.

A freestanding, PL/I-inspired systems language with compile-time macro
metaprogramming and controlled inline assembly, transpiling to
human-readable COR24 assembler source (.s).

## Status

Phase 4 in progress — code generation. Procedure codegen validated with
10-test suite covering empty procedures, RETURN with value in r0,
local variables with stack allocation (sub sp), parameter access at
fp+9, NAKED procedures (no prologue/epilogue), multiple procedures in
one program, multi-local frame layout, FREESTANDING option, byte-width
locals with sb, and combined globals+proc (.data + .text sections).
Static data emission validated with 10-test suite covering zero-fill
words/bytes, INIT values, multiple declarations, arrays, string
literals, and runtime store/load verification. Assignment codegen
validated with 10-test suite covering automatic/static variables,
byte-width stores, complex expressions, multiple sequential assignments,
large stack offsets, and negation. Expression codegen supports literal
loading (lc/la), variable load/store (fp-relative and static), binary
arithmetic (add, sub, mul), software division via __plsw_div, all
comparison operators, unary negate, and register allocation with spill.
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
