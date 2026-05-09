# GETMAIN/FREEMAIN Macros Saga

Extend PL/SW's MACRODEF system far enough to ship the PL/X-style
`?GETMAIN` / `?FREEMAIN` macros that the storage-allocation
design called for, then update `include/_plsw_storage.msw` to
provide them and rewrite the five storage demos to use them.

## Background

`include/_plsw_storage.msw` (storage-macros saga) ships only the
procedure interface today: callers write `P = _PLSW_GETMAIN(N);`
and `RC = _PLSW_FREEMAIN(P, N);`. The `?GETMAIN` / `?FREEMAIN`
macro spelling that `docs/storage-allocation.md` describes — and
that mirrors IBM OS/MVS — was deferred:

> PL/SW's MACRODEF/GEN system emits assembly only and can't
> synthesize the right code for arbitrary expression-typed
> LENGTH or arbitrary lvalue-typed ADDRESS arguments.

This saga removes that constraint.

The IBM-style invocation we want to support:

```pl/i
?GETMAIN(LENGTH(12), ADDRESS(P));    /* expands to: P = _PLSW_GETMAIN(12); */
?FREEMAIN(LENGTH(12), ADDRESS(P));   /* expands to: CALL _PLSW_FREEMAIN(P, 12); */
```

`LENGTH` is an integer expression (literal, identifier, or full
arithmetic expression). `ADDRESS` is an lvalue (identifier,
struct/pointer field, array element). The expansion has to
generate valid PL/SW source/code that handles either, then run
the existing parser + codegen on the result.

## Goal

After this saga: `?GETMAIN(LENGTH(<expr>), ADDRESS(<lvalue>))`
and the corresponding `?FREEMAIN` expand and compile correctly
for all the call shapes the storage demos exercise (literal
LENGTH, variable LENGTH, simple-identifier ADDRESS); the five
demos under `examples/storage_*.plsw` are rewritten to use the
macros; `just test` is 15/15 green against re-bootstrapped
goldens that capture the new (functionally identical) output.

## Approach: source-level macro expansion

PL/SW's existing `MACRODEF NAME; ... GEN DO; '...'; END; END;`
construct emits **assembly text** (each `'...'` line is one
assembly instruction) substituted into an `ASM DO` block. That's
fine for `?LED_SET` but can't synthesize a procedure call with
arbitrary expression args because it would have to re-implement
the compiler's expression evaluator inside MACRODEF.

The proposed fix is a parallel construct that emits **PL/SW
source text** instead of assembly:

```pl/i
MACRODEF GETMAIN;
    REQUIRED LENGTH(expr);
    REQUIRED ADDRESS(lvalue);
    EXPAND DO;
        '{ADDRESS} = _PLSW_GETMAIN({LENGTH});';
    END;
END;
```

`EXPAND DO` (working name) holds string fragments that are
PL/SW source after `{KEY}` substitution, not assembly. The
substituted source is re-fed to the lexer + parser — the rest
of the compiler handles it normally. Existing AST construction,
type checking, codegen, all unchanged.

Substitution rule: `{KEY}` is replaced by the **source-text
span** the caller wrote between the parentheses for that clause.
For `?GETMAIN(LENGTH(12), ADDRESS(P))`, the source spans are
`12` and `P`; for `?GETMAIN(LENGTH(n + 5), ADDRESS(STRUCT.FIELD))`,
they're `n + 5` and `STRUCT.FIELD`. Pure textual capture at the
call site, no AST construction needed for the args.

This is **token-level / source-level expansion**, not full
AST-level expansion. It's the minimum mechanism that makes
`?GETMAIN` / `?FREEMAIN` work the way they're spelled in IBM
docs, without committing to a Scheme-style hygienic macro
system.

`GEN DO` stays for the existing `?LED_SET` / `?GREET` / `?NOP`
macros (no migration needed). `EXPAND DO` is the new construct.

## Steps

### Step 001: macro-source-expansion

Compiler change in `src/`:

1. **Lexer**: add `TOK_EXPAND` for the new keyword. Source-span
   tracking on macro args: when parsing `?MACRO(KEY(<...>))`,
   record the source offset+length of the `<...>` span (between
   the inner `(` and `)`) so it can be substituted verbatim
   later.
2. **Parser** (`src/parser.h`, `src/macro.h`): MACRODEF body
   accepts `EXPAND DO; '...'; END;` in addition to (or instead
   of) `GEN DO; ...; END;`. Each EXPAND-DO line is a string
   literal that may contain `{KEY}` placeholders.
3. **Macro expander**: at `?MACRO(...)` invocation, walk the
   EXPAND-DO body, perform `{KEY}` -> captured-source-span
   substitution, emit the result back into the lexer's input
   stream (or, more simply, re-lex the substituted text inline
   and splice the resulting tokens into the surrounding parse).
4. **Test fixtures**: add a test suite in `src/main.c` that
   exercises `EXPAND DO` for both literal and expression args,
   simple-identifier lvalues, and (if time permits) complex
   lvalues like `STRUCT.FIELD`.
5. **Compatibility**: existing `GEN DO` macros continue to work
   unchanged. `?LED_SET`, `?GREET`, `?NOP` are unaffected.

Verification at end of step:
- New compiler-internal test suites pass (run via `just smoke`,
  pick a suite number, or via the existing `agentrail`'d test
  patterns in `main.c`).
- `just test` is still 15/15 green (no regression).

### Step 002: getmain-freemain-macros

Consume the new mechanism:

1. **Define macros** in `include/_plsw_storage.msw`:
   ```pl/i
   MACRODEF GETMAIN;
       REQUIRED LENGTH(expr);
       REQUIRED ADDRESS(lvalue);
       EXPAND DO;
           '{ADDRESS} = _PLSW_GETMAIN({LENGTH});';
       END;
   END;

   MACRODEF FREEMAIN;
       REQUIRED LENGTH(expr);
       REQUIRED ADDRESS(lvalue);
       EXPAND DO;
           'CALL _PLSW_FREEMAIN({ADDRESS}, {LENGTH});';
       END;
   END;
   ```
   (`?FREEMAIN` ignores the RC return for now — the demos that
   need RC continue to use the procedure form. Adding RC support
   needs an extra clause and is a follow-up if profiled need.)

2. **Rewrite the five storage demos** under `examples/storage_*.plsw`
   to use `?GETMAIN` / `?FREEMAIN` where they currently use
   `_PLSW_GETMAIN` / `_PLSW_FREEMAIN` directly. Demos that need
   RC return values (double_free, size_mismatch) keep the
   procedure form for those calls — the macro form covers the
   "I just want to allocate/free" case, the procedure form covers
   "I need to inspect the result."

3. **Re-bootstrap goldens**. Output should be functionally
   identical (same printed strings); golden files just get
   re-captured to reflect any line-count drift in the assembly
   diagnostics.

4. **Update docs/storage-allocation.md**: flip the "Macro layer
   (deferred)" section to "Macro layer (shipped)"; update the
   contract examples; remove the deferral note from
   `include/_plsw_storage.msw`'s header comment.

Verification:
- `just test` is 15/15 green.
- A diff of the 5 storage demos shows the macro form replaces
  the procedure form (for the alloc/free-without-RC cases).

## Out of scope

- Full AST-level / hygienic macros. Source-level capture is
  enough for `?GETMAIN` / `?FREEMAIN` and comparable
  expression-and-lvalue macros. Hygiene (avoiding identifier
  clashes between macro-internal names and caller scope) is a
  future concern; for now, document that macro-defined helpers
  should use a `_PLSW_` prefix convention.
- `RC(lvalue)` clause on `?GETMAIN` / `?FREEMAIN`. Mentioned in
  the design doc; deferred until profiled need.
- `SUBPOOL(expr)` clause. Reserved in IBM PL/X for multi-arena;
  COR24 doesn't need it.
- `?ESTAE` / try-catch macros. Future saga.
- Migrating `?LED_SET` / `?GREET` / `?NOP` to `EXPAND DO`. They
  work fine on `GEN DO`; no need to churn them.

## Rules

- The new `EXPAND DO` mechanism must NOT break the existing
  `GEN DO` mechanism. Backward compat is non-negotiable.
- No PL/SW language changes beyond the macro mechanism. No new
  syntax outside `MACRODEF`.
- Single-file edits to `include/_plsw_storage.msw` plus the five
  demos for step 002. No new files.
