# PL/SW Modular Compilation (GitHub issue #28)

Implement LIBRARY mode in the PL/SW compiler so large programs (e.g.
the SNOBOL4 interpreter) can be split across separately compiled
modules and linked with the existing FIXUP-based link24 toolchain in
components/linker/.

Reference: docs/linker-design.md (authoritative — supersedes the
literal text of issue #28, which predates the FIXUP design and still
mentions jump tables / EXTERNAL keyword / -m prefix flag; none of
those are needed in the current design).

## Approach

LIBRARY mode is activated by a `%DEFINE LIBRARY;` directive at the
top of a module's source. When set, the compiler suppresses:

1. Runtime preamble (_start, _UART_PUTCHAR, _UART_PUTS)
2. The MAIN wrapper procedure
3. ALL top-level DCL data emission (BASED records, globals, arrays)

External symbol references are NOT marked at compile time. The
existing meta-gen prep tool (components/linker/) scans the .s output,
detects unresolved `la rN,_SYMBOL` references, rewrites them to
`la rN,0` placeholders, and records FIXUPs in a .meta file. link24
patches the placeholders at link time.

## Existing state

- components/linker/{link24,meta-gen} already implemented (Rust)
- components/linker/tests/demo-fixup.sh exercises the linker on
  hand-written .s fixtures
- Uncommitted working-tree changes in src/codegen.h and src/main.c
  already implement LIBRARY-mode suppression — these need review,
  testing, and commit
- Issue #30 (DEF_MAX overflow) just fixed in commit 72a7111

## Steps

1. Review and commit the in-progress LIBRARY mode suppression in
   src/main.c and src/codegen.h
2. Add a PL/SW-driven end-to-end multi-module test (small fixture)
   exercising compile -> meta-gen prep -> assemble -> meta-gen emit
   -> layout -> reassemble -> link24 -> run on emulator
3. Build the SNOBOL4 interpreter (sw-cor24-snobol4) modularly using
   the new flow; verify the dating-app demo runs
4. Update docs/usage.md with a "Modular compilation" section pointing
   at docs/linker-design.md
5. Close GitHub issue #28
