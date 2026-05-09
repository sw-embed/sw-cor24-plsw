# Emit Zero-Fill Saga

Per `tools/briefs/dcpls-emit-zero-fill.md`. dcsno is blocked
because `sno_main.s` is 261,638 bytes (97.7% enumerated zero-fill
text), maxing out PL/SW's 256 KB `EMIT_BUF` and preventing any
new feature that touches `snoglob.msw`. The dcxas partner brief
(`pr/zero-fill-directive`) shipped, so `cor24-asm` now accepts
`.zero N`. This saga makes PL/SW codegen consume the directive
for `INIT(0)` static arrays.

## Goal

When emitting a top-level static array whose initializer is
all-zero, emit `.zero <total_bytes>` instead of enumerating
`.byte 0,0,0,...` for each element. Same `.bin` bytes; smaller
`.s`. After this lands and mike reinstalls the PL/SW compiler:

* `sno_main.s` drops from ~261 KB to ~7 KB.
* `EMIT_BUF` utilisation drops from 99.8% to ~3%.
* dcsno's `pr/sno-engine-consolidation` (parked) restarts.

## Scope

Touch only `cg_emit_static_var` (and any helpers it uses) in
`src/codegen.h`. Detect the "all-zero" case before entering the
per-element emit loop:

* Explicit `INIT(0)` on a BYTE/INT/PTR/WORD/CHAR array.
* No `INIT` clause at all (implicit zero for static storage).
* `INIT('')` empty-string CHAR array (likely zero-bytes; verify).
* Mixed-init arrays like `INIT(0, 1, 2, 0, 0)` keep the
  spelled-out form -- any non-zero element disables the fast
  path.

`total_bytes = array_count * element_width` (1 for BYTE/CHAR,
3 for INT/PTR/WORD).

## Verification

1. **Byte-identical**: `examples/chain.plsw` (has INIT(0)
   ARENA(512) BYTE) produces a `.bin` byte-identical to the
   pre-change output.
2. **Compiler unit tests** in `src/main.c`: add a suite with
   `DCL X(100) BYTE INIT(0);` and verify the emitted `.s`
   contains `.zero 100` (and not `.byte 0,...`).
3. **Reg-rs**: `just test` 15/15 green. Goldens for `.out`/`.rgt`
   should be unchanged (program output identical); `.err` will
   shift because the assembly line count drops for chain.plsw.
   Re-bootstrap and inspect.
4. **Round-trip**: confirm `cor24-emu --lgo` runs the emitted
   `.lgo` correctly (already covered by reg-rs running each
   demo).

## Steps

1. **emit-zero-fill**. Single step:
   - Locate `cg_emit_static_var` in `src/codegen.h`.
   - Add an "all-zero init?" predicate: returns true when (a)
     no INIT, OR (b) every INIT element is integer literal 0.
   - When the predicate is true, emit `.zero <total_bytes>`
     and skip the per-element loop.
   - Add a compiler test suite in `src/main.c` covering the
     positive and negative cases (all-zero, mixed, no-init,
     non-zero string).
   - Re-bootstrap reg-rs goldens after running the change
     against the demos. Inspect each per the doc.
   - Update `docs/storage-allocation.md` with a brief note (if
     that doc references emission) -- probably not needed.
   - `just test` 15/15 green.

## Out of scope

- Changes to `cor24-asm` (dcxas's brief; already shipped).
- LIBRARY-mode DCL suppression -- stays the same; this fix only
  affects entry-module emission.
- Non-zero init optimization (`.byte 1,2,3` stays as-is).
- Anything beyond `cg_emit_static_var`.
