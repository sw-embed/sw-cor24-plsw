# ast-to-chunks (Phase 2b of shrink-lgo)

Migrate the AST node pool from 10 × 12,288-slot parallel static
arrays to chunk-backed storage drawn from the 64 KB pool that
the `chunk-allocator` saga shipped.

This is **the largest single shrink** in the whole shrink-lgo
plan. Per `docs/shrink-lgo-size.md`, the AST pool is ~360 KB of
`.data` (56% of the total). After this saga, the AST static
cost drops to ~50 bytes of metadata; in exchange, AST consumes
1-N chunks dynamically (where N is determined by Phase 0
measurement). Net projected: ~360 KB - 64 KB pool already paid
= **~296 KB net shrink** of `plsw.lgo`.

## Target

After this saga, `plsw.lgo` should be in the **~1.35-1.4 MB**
range (down from the current 1.66 MB), unblocking dcsno's
saga-expr-completeness step 003 and the downstream dcftn work
that depend on PL/SW fitting in COR24's 1 MB SRAM with room
for runtime.

## Sizing

* Per chunk: `CHUNK_SIZE = 4096` chars. 10 fields × 3 bytes per
  node = 30 bytes/node. So **~136 nodes per chunk**.
* `CHUNK_MAX = 16` total chunks. If used solely by AST (worst
  case), that gives **2,176-node ceiling** -- much smaller than
  today's `NODE_POOL_MAX = 12288` cap. Phase 0 measurement
  decides whether 16 chunks is enough or `CHUNK_MAX` needs
  bumping. Measurement-driven, not aspirational.

## Layout decision

**Struct-of-arrays per chunk.** Each chunk holds parallel
sub-arrays for the 10 fields, indexed by slot. Macros translate
node-id `i` to `(chunk_idx = i / N, slot = i % N)` once, then
field access is `chunks[chunk_idx]->kind[slot]` etc. This:

* Keeps the parallel-array shape callers already use.
* Localizes the change to `src/ast.h` (and the macro layer).
* Avoids touching every field reference's structural form.

## Steps

1. **measure-ast**. Add `#ifdef MEASURE` instrumentation in
   `nd_alloc()` to track `peak_nd`. Build a one-off measurement
   binary, run it across representative inputs (lexer/parser
   suites in test_main, hello.plsw, hello_macro.plsw, sno_exec
   if present, plsw self-compile). Capture peak node counts in
   `docs/memory-audit-2026-05-10.md`. Decide: does 16 chunks
   (~2,176-node ceiling) suffice? If not, document the bump and
   apply it to `src/chunk.h` in the same step.

2. **ast-accessors**. Introduce `nd_kind(i)` / `nd_type(i)` /
   `nd_stor(i)` / `nd_level(i)` / `nd_left(i)` / `nd_right(i)` /
   `nd_next(i)` / `nd_ival(i)` / `nd_line(i)` / `nd_name(i)`
   accessor macros in `src/ast.h`. Initially they resolve to the
   existing parallel arrays (`#define nd_kind(i) nd_kind_arr[i]`
   or similar -- pick a naming that doesn't collide). Mechanical
   replacement of all ~391 callsites across `src/types.h`,
   `src/parser.h`, `src/layout.h`, `src/ast.h`, `src/codegen.h`,
   and `src/test_main.c`. No storage change yet -- proves the
   macro layer compiles and tests pass before disturbing storage.

3. **ast-chunk-storage**. Switch the macros to chunk-backed
   storage. Each chunk holds an `ast_block` struct-of-arrays.
   `nd_alloc()` allocates the next slot in the current chunk;
   when the chunk fills, `chunk_alloc()` for a new one;
   `ast_init()` returns all AST chunks to the pool. The static
   parallel arrays `nd_kind_arr[NODE_POOL_MAX]` etc. are
   removed. Tests must continue to pass, including the chunk
   suite (#37) and reg-rs (15/15).

4. **ast-baseline**. `just clean && just build-lgo &&
   just build-test-lgo`. Capture new sizes + SHAs. Update
   `docs/shrink-lgo-size.md` Phase 2b -> DONE with measured
   numbers. Add CHANGES.md entry flagging this as a **rebuild
   recommended** entry for downstream consumers (binary behavior
   changes only at the limit -- chunk overflow message replaces
   pool overflow -- but the .lgo bytes are different and dcsno
   should refresh its installed plsw.lgo). Close the saga.

## Risks

* Indexing performance: `i / N` and `i % N` where N=136 is not
  power of two. tc24r doesn't strength-reduce divisions. The
  brief flagged this. If measurement after step 3 shows
  unacceptable slowdown, consider:
  - Round N down to a power-of-two-friendly value at the cost
    of underutilizing each chunk (e.g. N=128 -> wastes 8 nodes
    per chunk = 240 bytes total).
  - Cache `(chunk_idx, slot)` separately rather than recomputing
    on every access.
* Mechanical-replacement scope: 391 callsites is a lot. A single
  missed `nd_kind[i]` becomes a build error after step 2 (good
  -- catches it immediately).
* Reset hygiene: if `ast_init()` doesn't release chunks back to
  the pool, repeated compiles in REPL mode leak chunks until
  exhaustion. Test this explicitly in step 3.

## What does NOT go in this saga

* Migrating other pools (`arena_buf`, `src_buf`, `inc_buf`,
  `mac_gen_buf`) to chunks -- that's the `buffer-to-chunks`
  saga (Phase 3), only if measurements demand more headroom
  after this saga lands.
* Lint script enforcing the 4 KB ceiling -- that's
  `static-lint` (Phase 5).
* tc24r changes.
