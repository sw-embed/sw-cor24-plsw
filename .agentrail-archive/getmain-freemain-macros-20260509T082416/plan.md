# GETMAIN/FREEMAIN Macros Saga

Extend PL/SW's MACRODEF system to ship the IBM PL/X-style
`?GETMAIN` / `?FREEMAIN` macros that the storage-allocation
design called for. Educational project, not production —
backward compatibility with existing GEN-DO macros is negotiable.

## Background

`include/_plsw_storage.msw` (storage-macros saga) ships only the
procedure interface: `P = _PLSW_GETMAIN(N);` and
`RC = _PLSW_FREEMAIN(P, N);`. The PL/X-style macro spelling

```pl/i
?GETMAIN(LENGTH(12), ADDRESS(P), RC(rc));
?FREEMAIN(LENGTH(12), ADDRESS(P), RC(rc));
```

was deferred because PL/SW's `MACRODEF` / `GEN DO` mechanism
emits assembly text only, and assembly substitution can't
synthesize the right code for arbitrary expression-typed
`LENGTH` (literal works; runtime variable doesn't) or
arbitrary lvalue-typed `ADDRESS`. This saga removes that
limitation.

## Design (after user-feedback round)

### Distinguish `GEN DO` (source) from `GEN ASM DO` (assembly)

PL/X's actual macro semantics: `GEN` blocks emit PL/X source by
default; `GEN ASM` blocks emit assembler. PL/SW's existing
`GEN DO` behaves as `GEN ASM` (assembly emission). We align with
PL/X by:

- **Renaming existing `GEN DO` → `GEN ASM DO`** (assembly-text
  emission, single-quoted-string lines). Migrate the three
  existing macros (`?LED_SET`, `?GREET`, `?NOP`) to the new
  spelling. Three one-word edits across three .msw files.
- **`GEN DO` (default) becomes source-text emission.** Body is
  PL/SW source statements with `{KEY}` placeholders for arg
  substitution. Substituted text is re-fed to the lexer/parser
  so the rest of the compiler handles type-checking, codegen,
  etc., normally.

This is a backward-compat break and the user explicitly said
that's OK ("backward compat IS negotiable, this is educational,
not production"). The migration is mechanical and contained.

No new keywords introduced — `GEN`, `ASM`, `DO`, `END` all
already exist in PL/SW.

### Macro body shape after the change

```pl/i
/* Source-emitting macro (default) */
MACRODEF GETMAIN;
    REQUIRED LENGTH(expr);
    REQUIRED ADDRESS(lvalue);
    REQUIRED RC(lvalue);
    GEN DO;
        {ADDRESS} = _PLSW_GETMAIN({LENGTH});
        IF ({ADDRESS} = 0) THEN {RC} = 4;
        ELSE {RC} = 0;
    END;
END;

/* Assembly-emitting macro (existing pattern, now with explicit ASM) */
MACRODEF LED_SET;
    REQUIRED VAL(expr);
    GEN ASM DO;
        'la      r2,16711680';
        'lc      r0,{VAL}';
        'sb      r0,0(r2)';
    END;
END;
```

`?GETMAIN(LENGTH(12), ADDRESS(P), RC(rc))` expands (textually)
to:

```pl/i
P = _PLSW_GETMAIN(12);
IF (P = 0) THEN rc = 4;
ELSE rc = 0;
```

…which the existing parser + codegen handle without any AST
change.

### Substitution rule

`{KEY}` in the template is replaced by the **source-text span**
the caller wrote between the parentheses for that clause. For
`?GETMAIN(LENGTH(12), ADDRESS(P), RC(rc))` the spans are `12`,
`P`, and `rc`. For `?GETMAIN(LENGTH(N + 5), ADDRESS(STRUCT.FIELD), RC(rc))`
the spans are `N + 5`, `STRUCT.FIELD`, `rc`. Pure textual
capture at the call site — no AST construction for the args, no
type analysis at substitution time.

The substituted text is then re-tokenized and spliced into the
parse stream where the `?MACRO(...)` invocation lived.

### Long-term direction (NOT v1 scope, but constrains design choices)

The user wants macros to eventually become a real compile-time
preprocessor: conditional logic (`%IF`/`%ELSE`), loops (`%DO`?),
nested macro calls with finite recursion depth (PL/X allows 255
levels by default; we'll cap lower — say 32). The macros "run"
at compile time to generate code.

v1 implementation choices that DON'T preclude this vision:

1. **Source-level / re-tokenization architecture.** The macro
   expansion pass is conceptually:
   ```
   capture template -> substitute {KEY} -> re-tokenize -> parse
   ```
   Adding a `%IF`/`%DO` evaluator inside the substitution step
   is a natural extension; it modifies the template before
   re-tokenization rather than redesigning the pipeline.

2. **Substitution is recursive.** The implementation walks the
   template character-by-character, handling `{KEY}` and (in
   v2) `%IF`/`%DO`. Recursive calls handle nested `?MACRO()`
   invocations in the expanded text. v1 implements basic
   substitution + tokenization; depth tracking is plumbed in
   from day one (parameter on the expand function) even though
   v1 only ever calls with depth=1. Adding the recursion-limit
   check later is one branch.

3. **Captured arg spans are stored as offset+length into a
   stable buffer**, not as parsed AST. The same span can be
   substituted multiple times (e.g., a macro template
   referencing `{ADDRESS}` twice). Substituting AST nodes would
   require deep-copying the AST or risking aliasing; spans
   sidestep that.

4. **Depth counter in the expansion function signature** even
   though v1 uses it only to assert "depth < 32 always." Future
   nested-macro support uses the same parameter.

5. **Template body parser doesn't fully tokenize at MACRODEF
   time**; it just records the source span between the
   `GEN DO;` and matching `END;`. This means nested directives
   inside the template (future `%IF` etc.) don't need special
   handling at MACRODEF parse — they're just text until
   expansion time.

The non-goal "v1 doesn't have to support nested ?MACRO calls"
is OK because the storage demos don't need it. But the
implementation must NOT actively prevent v2 from adding nesting.

## Goal

After this saga: `?GETMAIN(LENGTH(<expr>), ADDRESS(<lvalue>),
RC(<lvalue>))` and the corresponding `?FREEMAIN` macro work for
all the call shapes the storage demos need (literal LENGTH,
runtime-variable LENGTH, simple-identifier ADDRESS,
simple-identifier RC). The five demos under
`examples/storage_*.plsw` use the macro form. `just test` is
15/15 green against re-bootstrapped goldens. Existing macros
(`?LED_SET`, `?GREET`, `?NOP`) are migrated to `GEN ASM DO` and
continue to work.

## Steps

### Step 001: macro-template-expansion

Compiler change in `src/`:

1. **Lexer**: track source-text spans for macro args. When
   parsing `?MACRO(KEY(<inner>))` invocations, record the offset
   and length of the `<inner>` span. Add `TOK_ASM` (if not
   already) so `GEN ASM DO` parses cleanly.
2. **Parser** (`src/parser.h`, `src/macro.h`):
   - `MACRODEF` body parses two flavors: `GEN DO; ... END;`
     (source template) and `GEN ASM DO; ... END;` (assembly
     template, the existing behavior). The flag is stored on
     the macro definition so expansion knows which mode.
   - For source templates: capture the body as a SOURCE SPAN
     (offset+length into the input buffer) plus a list of
     `{KEY}` placeholder positions. Don't fully tokenize at
     MACRODEF time.
   - For assembly templates: keep the existing per-line
     string-literal capture. (Or unify the two via spans if
     trivial, but unification isn't required for v1.)
3. **Macro expander**:
   - Expansion takes a depth parameter (`int depth`); v1 calls
     with `depth=1`. Add an `assert(depth < MACRO_MAX_DEPTH)`
     where `MACRO_MAX_DEPTH = 32`. v2 will increment-and-check
     before recursive calls.
   - At `?MACRO(...)` invocation:
     - Capture each arg's source span at the call site (the
       text inside the inner parens).
     - For source templates: walk the body span, copy verbatim,
       substitute `{KEY}` placeholders with the captured spans.
       The result is a freshly-allocated text buffer.
     - For assembly templates: existing behavior — emit lines
       inside an `ASM DO` block.
     - Splice the result back into the lexer/parser's input
       stream so the rest of the parse picks it up.
4. **Migrate existing `GEN DO` → `GEN ASM DO`** in:
   - `examples/greet.msw`
   - `examples/system.msw` (both `?LED_SET` and `?NOP`)
   - The MACRODEF test fixtures in `src/main.c`'s suites that
     use `GEN DO;` for assembly.
5. **New compiler-internal tests** in `src/main.c`:
   - Source template expansion with literal expression.
   - Source template expansion with variable expression.
   - Source template expansion with simple-identifier lvalue.
   - Source template expansion with two args, both used twice
     in the body (verify span re-use works).
   - Existing `GEN ASM DO` tests still pass.
6. **Verification**:
   - Compiler-internal suites green.
   - `just test` 15/15 green (no regression on the .plsw demos).
   - The existing `hello-macro` and `chain` recipes still work
     end-to-end.

### Step 002: getmain-freemain-macros

Consume the new mechanism:

1. **Define macros** in `include/_plsw_storage.msw` using the
   new `GEN DO` (source) form:
   ```pl/i
   MACRODEF GETMAIN;
       REQUIRED LENGTH(expr);
       REQUIRED ADDRESS(lvalue);
       REQUIRED RC(lvalue);
       GEN DO;
           {ADDRESS} = _PLSW_GETMAIN({LENGTH});
           IF ({ADDRESS} = 0) THEN {RC} = 4;
           ELSE {RC} = 0;
       END;
   END;

   MACRODEF FREEMAIN;
       REQUIRED LENGTH(expr);
       REQUIRED ADDRESS(lvalue);
       REQUIRED RC(lvalue);
       GEN DO;
           {RC} = _PLSW_FREEMAIN({ADDRESS}, {LENGTH});
       END;
   END;
   ```
   `RC(lvalue)` is **required** in v1 (simpler than supporting
   `%IF DEFINED(RC)` inside templates). Future enhancement:
   make RC optional once `%IF` works inside source templates.

   IBM-style RC values:
   - `?GETMAIN`: 0 = success, 4 = out of memory.
   - `?FREEMAIN`: 0 = success, 1 = double-free / invalid,
     2 = size mismatch (passes through `_PLSW_FREEMAIN`'s
     return).

2. **Rewrite the five storage demos** under
   `examples/storage_*.plsw` to use `?GETMAIN` / `?FREEMAIN`.
   `storage_double_free` and `storage_size_mismatch` already
   inspect RC; the macro form covers both. `storage_basic`,
   `storage_coalesce`, `storage_oom` get an RC variable they
   didn't previously check; that's fine — the new code is
   strictly more idiomatic.

3. **Re-bootstrap goldens** for the five `plsw_storage_*` cases.
   Output should be functionally identical (same printed strings);
   golden files re-capture to reflect any line-count drift in
   stderr diagnostics.

4. **Update `docs/storage-allocation.md`**: flip "Macro layer
   (deferred)" → "(shipped in v1)"; update the contract examples
   to show RC-required form. Remove the deferral note from
   `include/_plsw_storage.msw`'s header comment.

5. **Update the dcsno brief** at
   `tools/briefs/dcsno-storage-allocation-runtime.md` to mention
   that the macro spelling is now available (in addition to the
   procedure form).

Verification:
- `just test` is 15/15 green.
- Diff of the five demos shows the macro form replacing direct
  `_PLSW_GETMAIN` / `_PLSW_FREEMAIN` calls.

## Out of scope

- Full AST-level / hygienic macros. Source-level capture covers
  this saga's needs. Hygiene is a future concern.
- Conditional `%IF`/`%ELSE` *inside* MACRODEF templates. v1
  treats the template as inert text (with `{KEY}` substitution
  only). The implementation is structured to add this later.
- Loops (`%DO` inside templates). Same.
- Nested `?MACRO()` calls inside macro expansions. Same — depth
  parameter is wired but always called with `depth=1` for now.
- Optional `RC` (require it in v1; make optional in a future
  saga once `%IF` works in templates).
- `SUBPOOL` clause. Reserved in IBM PL/X for multi-arena;
  COR24 doesn't need it ever.
- `?ESTAE` / try-catch macros. Future saga.

## Rules

- No new keywords introduced. `GEN`, `ASM`, `DO`, `END` already
  exist in PL/SW.
- The existing `GEN DO` (assembly) is renamed to `GEN ASM DO`;
  the three existing macros migrate. This is a backward-compat
  break, allowed because the project is educational.
- Implementation choices in step 001 must not preclude the
  long-term preprocessor vision (conditionals, loops, recursion)
  — see "Long-term direction" above.
- `just test` stays green at every step boundary.
