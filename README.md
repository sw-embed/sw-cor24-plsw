# sw-cor24-plsw

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA.

A freestanding, PL/I-inspired systems language with compile-time macro
metaprogramming and controlled inline assembly, transpiling to
human-readable COR24 assembler source (.s).

## Status

Phase 6 in progress — control flow code generation. DO I = start TO end
counted loops lowered to while-equivalent (init, compare via cls, body,
increment, jump back). IF/THEN/ELSE and DO WHILE codegen complete.
Phases 0-5 complete: lexer, parser, AST, symbol table, type system,
storage layout, emitter framework, expression/assignment codegen, static
data, procedure prologue/epilogue, call codegen with R-to-L args, and
recursion support.

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
