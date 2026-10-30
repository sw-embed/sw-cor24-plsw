# Migrating the PL/SW Compiler from C to HLASM

Status: plan (2026-10-30). No migration work has started.

## Goal

Today the PL/SW compiler is written in C (`src/*.h`, `src/main.c`) and
built by tc24r, a host-side cross-compiler. The long-term goal is a
PL/SW compiler written in PL/SW. This plan covers the intermediate
step: porting the compiler from C to HLASM source assembled by
[sw-cor24-hlasm](../../sw-cor24-hlasm), which runs on COR24 itself.

The resulting bootstrap chain needs no host compiler:

```
hlasm.s (hand-written COR24 asm)
   -> assembles plsw-in-HLASM          (this plan, phases 1-4)
      -> compiles plsw-in-PL/SW        (phase 5, separate saga)
         -> compiles itself (fixpoint)
```

Why go through HLASM instead of porting C straight to PL/SW:

- It removes tc24r from the trust and bootstrap chain. Every stage
  after `hlasm.s` is built on COR24.
- The C-to-HLASM port is mechanical. Each C function maps to one HLASM
  routine that follows tc24r's calling convention, so the result can
  be checked against tc24r's output one function at a time.
- sw-cor24-hlasm's active `tinyc-macro-ports` saga is already proving
  exactly this porting pattern on the tiny-C demos. This plan reuses
  that work rather than inventing a new method.

## Precursor: sw-cor24-hlasm `tinyc-macro-ports`

That saga ports `../sw-cor24-x-tinyc/demos` from C to HLASM. Its
steps, and what each one gives this plan:

| hlasm step | Delivers | Needed here for |
|---|---|---|
| 1 structured-jmp-relaxation | `IF`/`DO`/`SELECT` output that `cor24-asm` accepts at any branch distance | Large routines (codegen has 45 functions in 1,747 lines of C) |
| 2 e2e-run-harness | `scripts/hlasm-e2e.sh` (hlasm -> .s -> asm -> run) | The build and test loop |
| 3 tinyc-survey-and-macro-catalog | `docs/tinyc-port-plan.md`: idiom catalog and macro gaps | The C-idiom-to-macro mapping |
| 4 macro-library-core | `lib/` COPY members: PROLOG/EPILOG/CALL/RET, UART, data | Shared library for every compiler module |
| 5-7 port demos | Proof on I/O, control flow, functions, arrays, structs, strings | Confidence that every construct in `src/` has a port pattern |
| 8 macro-value-showcase | C vs tc24r `.s` vs HLASM side by side | Size and readability baseline |

Phase 0 of this plan waits for steps 1-4 at minimum. Steps 5-7 should
land before phase 2 starts.

## Source Layout: Single Translation Unit, Like Today

The C compiler has no libraries and no separate compilation. `main.c`
includes every module header, the headers hold all function bodies,
and tc24r emits a single `plsw.s`. The HLASM version keeps the same
shape:

```
hlasm/
  plsw.hlasm          main member: COPY each module in dependency order
  str.hlasm           one member per C header, same names
  arena.hlasm
  chunk.hlasm
  io.hlasm
  token.hlasm
  lexer.hlasm
  ast.hlasm
  symtab.hlasm
  types.hlasm
  layout.hlasm
  emit.hlasm
  parser.hlasm
  macro.hlasm
  codegen.hlasm
  compile.hlasm
  plsw.sourceset      source set naming the main member and all COPY members
lib/                  vendored macro library from sw-cor24-hlasm
```

hlasm already supports this through `COPY`/`INCLUDE` named members and
`.sourceset` manifests (`SOURCESET`, `PROFILE`, packed extra buffers).
One hlasm run reads all members and writes one `.s`, so no linker is
needed, which matches the current C build.

Differences from C that the port has to handle:

- **One global label namespace.** C `static` helpers and locals are
  scoped; HLASM labels are not. Prefix every routine and data label
  with its module (`lex_`, `cg_`, ...). tc24r already emits global
  `_name` labels, so keeping the C names is the default.
- **Include guards.** The C headers use `#ifndef X_H` guards. In HLASM,
  either list each member exactly once in `plsw.hlasm`, or use an
  `IFNDEF`/`SET` guard per member. Listing once is simpler and enough.
- **Capacity.** Every member must fit in hlasm's source window at the
  same time, and every routine label, macro and `SET` symbol shares one
  set of tables. See blockers B1-B3 below.

## Phases

Each phase ends with every regression check green (see Verification).

### Phase 0: hlasm readiness

Fix the hlasm blockers listed below, in sw-cor24-hlasm, before any
compiler code is ported. Exit gate: hlasm assembles a synthetic,
compiler-sized source (roughly the expected line, label and macro
counts) split across ~16 COPY members, and `cor24-asm` accepts the
output.

### Phase 1: Freeze and baseline

- Freeze language feature work in the C compiler. Bug fixes still go
  into C and are ported along with everything else.
- Build the golden corpus: the `.s` output of the current C compiler for
  every `reg-rs` test, every `examples/*.plsw`, and the large
  downstream sources (e.g. sw-cor24-snobol4's `sno_engine.plsw`). The
  HLASM compiler must reproduce these byte for byte.
- Record the baseline: `plsw.s` size, `.lgo` size, cycle counts per
  golden, peak stack depth.
- Vendor hlasm (`hlasm.s`, `lib/`, the source-set runner) under
  `vendor/` following [vendor-plan.md](vendor-plan.md).

### Phase 2: Port the runtime modules (leaves)

Port in dependency order, leaves first:

1. `str` (8 functions), `arena` (5), `chunk` (4), `io` (7)
2. `token` (5), `lexer` (32)
3. `ast` (15), `symtab` (11), `types` (21), `layout` (8)

Hybrid build while porting: the build uses tc24r's `plsw.s` with the
ported functions removed, plus hlasm's output for those functions.
This works because the HLASM routines follow tc24r's calling convention
(args on the stack right to left, result in r0, `push fp/r2/r1`
prologue, first arg at `fp+9`) and keep tc24r's `_name` labels. Add a
`scripts/splice-plsw.sh` that drops ported functions from the tc24r
`.s` by label range and appends the HLASM output.

After each module: the golden corpus is byte-identical and the
`test_main.c` unit tests for that module still pass. The unit tests
remain C, built against the hybrid, until phase 4.

### Phase 3: Port the compiler core

4. `emit` (32)
5. `parser` (23)
6. `macro` (27)
7. `codegen` (45). This is the largest module. Split it into several
   COPY members by statement or expression family if routine sizes
   push structured-branch relaxation or table limits.
8. `compile` (5), `main.c` (buffers, UART protocol, mode dispatch)

Large static buffers (`src_buf` 192 KiB, `inc_buf` 32 KiB) become
`.space` reservations, which hlasm passes through to the output.

### Phase 4: Cut over

- `just build` runs hlasm instead of tc24r. Remove tc24r from
  `vendor/active.env` and the justfile.
- Replace `src/test_main.c`. Turn each unit test into a reg-rs test that
  drives the compiler through its UART protocol, or port the harness
  to HLASM. Choose per test.
- Move `src/` to `archive/c-src/` (kept for reference until phase 5
  ends) and update README, CLAUDE.md, `docs/architecture.md`,
  `docs/usage.md`.
- Acceptance: the golden corpus is byte-identical; `.lgo` size and
  cycle counts are within an agreed margin of the C baseline (proposed:
  +10% size, +10% cycles); peak stack fits the EBR stack.

### Phase 5: PL/SW self-hosting (separate saga, outline only)

- Use the HLASM-built compiler as stage 1 to compile a PL/SW rewrite
  (stage 2). Stage 2 compiles itself (stage 3). Accept when
  `stage2.s == stage3.s`.
- Language gaps to plan for before starting: deep `ELSE IF` chains
  exhaust the compiler stack (issue #33, so use `SELECT`), and
  separate compilation relies on `link24`, a host-side Rust tool.
- The HLASM compiler stays as the bootstrap seed and is frozen once
  stage 2 is accepted.

## hlasm Blockers and Gaps

Taken from sw-cor24-hlasm `docs/design.md`, `docs/architecture.md`,
`docs/bootstrap.md` and the data layout in `hlasm.s` as of
2026-10-30. Items marked *verify* need a test in hlasm to confirm.

### Must fix before phase 2

| ID | Gap | Why it blocks | Evidence |
|---|---|---|---|
| B1 | Runtime tables are sized for demos. The runtime arena is `0x0C0000-0x0C055E` (~1.4 KB) and holds the macro table and body pool, symbol table and conditional stack. | The compiler port needs hundreds of routine labels, dozens of macros and many `EQU` field offsets. | `docs/architecture.md` memory table; `_symbol_table` in `hlasm.s` |
| B2 | The source window is `0x080000-0x0BFFFF` (256 KiB) for all members together, and the default main buffer is 4096 bytes. | The C source is ~7,300 lines without tests; the HLASM port will be several times that. | `docs/bootstrap.md` memory map |
| B3 | Structured nesting is limited to 8 levels each for `IF`, `DO` and `SELECT`, and the conditional-assembly stack is shallow. | The parser and codegen nest deeper than 8 in places. *verify* the deepest nesting in `src/` | `hlasm.s` structured-state comments |
| B4 | Structured `IF`/`DO`/`SELECT` output is not yet accepted by `cor24-asm` at long distances. | Every non-trivial routine uses structured control flow. | hlasm saga step 1 (in progress) |
| B5 | Macro bodies do not re-evaluate structured or conditional-assembly directives. | Macros like `FOR_EACH`, `IF_NULL`, or a `CALL` macro whose expansion depends on argument count cannot be written. | `docs/design.md` macro syntax notes |
| B6 | Default macro parameter values. *verify* whether supported. | `CALL fn,a,b,c` and `PROLOG` with an optional frame size need defaults or varargs. | hlasm saga step 3 asks the same question |
| B7 | No end-to-end harness. | Needed for the hybrid build loop and goldens. | hlasm saga step 2 |

### Likely needed during phases 2-3

| ID | Gap | Notes |
|---|---|---|
| G1 | Structured conditions take a single compare (`cc_eq`, `cc_ne`, `cc_lt`, `cc_lu`, `cc_zset`, `cc_zclr`). There is no `AND`/`OR`, and no `>`/`<=` forms. | C `&&`/`||` must be lowered by hand into nested `IF`s or `DOEXIT` chains. Compound conditions would cut much of that noise. |
| G2 | `SET`/`EQU` expressions allow only `+` and `-`. | Struct field offsets and sizes (`ast`, `symtab`, `layout`) want `*` for array strides at least. |
| G3 | Struct layout helpers (an HLASM `DSECT`-style block or `FIELD name,size` macro that accumulates offsets). | Without this, every node and symbol field offset is a hand-written `EQU`. |
| G4 | String literal emission with escapes (`\n`, `\t`, `\\`, `\"`, `\0`). *verify* what `.ascii`/`.asciz` passthrough handles. | Hundreds of compiler messages and emitted asm fragments are C strings. |
| G5 | The 128-byte line buffer. | Long `.byte` tables and long emitted-asm string data may exceed it. |
| G6 | Diagnostics that point at the original member and line for errors inside `COPY` members. | `HLDIAG` already tracks a source id and line; it needs to cover every error path at this scale. |
| G7 | Assembly time. hlasm is two-pass and runs on the emulator. | Measure cycles for a full compiler build in phase 0, and set a budget so the edit-build-test loop stays usable. |

When a port step hits a gap that is not listed here, stop. Add the
gap to this table and fix it in sw-cor24-hlasm as its own step, rather
than working around it in the port. The hlasm saga follows the same
rule.

## Verification

At every step:

- `just test` (reg-rs): all goldens pass.
- Golden corpus: the compiler's `.s` output is byte-identical to the C
  baseline for every input.
- Unit tests (`test_main.c` against the hybrid build) pass until
  phase 4 replaces them.
- Size and cycle report compared with the phase 1 baseline, flagging
  regressions over the agreed margin.

## Risks

- **Capacity growth in hlasm (B1-B3)** could turn into a redesign, for
  example moving tables to a heap arena. This should be found in
  phase 0, not partway through phase 3.
- **Code size.** Macro-expanded HLASM may be larger than tc24r's output.
  The compiler image plus 224 KiB of buffers has to fit in 1 MB of
  SRAM next to hlasm's own load windows during builds.
- **Stack depth.** The parser and codegen recurse. Hand-written frames
  can be smaller than tc24r's, but deep recursion still has to fit the
  EBR stack (see issue #33).
- **Two codebases during the port.** The C freeze in phase 1 limits
  this; bug fixes have to land in both until phase 4.

## Proposed Saga Steps

One AgentRail step per line, with each module step small enough for
one branch:

1. `hlasm-blockers-b1-b3`: raise hlasm table, window and nesting
   limits (in sw-cor24-hlasm)
2. `hlasm-blockers-b5-b6`: macro-body re-evaluation and default
   parameters (in sw-cor24-hlasm)
3. `hlasm-scale-proof`: synthetic compiler-sized source set; measure
   build cycles (G7)
4. `golden-corpus-baseline`: phase 1 corpus, size and cycle baseline
5. `vendor-hlasm`: vendor hlasm and `lib/`, add `hlasm/plsw.sourceset`
6. `hybrid-splice-build`: `scripts/splice-plsw.sh` plus a just target
7. `port-str-arena-chunk-io`
8. `port-token-lexer`
9. `port-ast-symtab-types-layout`
10. `port-emit`
11. `port-parser`
12. `port-macro`
13. `port-codegen` (may split into 2-3 steps)
14. `port-compile-main`
15. `cutover-remove-c`
16. `plsw-self-host-plan`: write the phase 5 plan as a new saga

## Open Questions

- Should the HLASM sources live in `hlasm/` in this repo (as proposed)
  or under `src/` once the C is removed?
- Is byte-identical `.s` output the right acceptance bar, or should
  semantically equivalent output (same program behavior) be allowed so
  the port can improve codegen along the way? This plan assumes
  byte-identical, so the port changes nothing.
- Should `components/linker` (Rust, host-side `link24`) be part of the
  native chain, or stay a host tool until phase 5?
