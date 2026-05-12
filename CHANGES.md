# Changes

Notable PL/SW changes that affect the shipped `plsw.lgo`. Not a
full changelog -- only entries that signal "rebuild and reinstall
the compiler" are recorded here.

## 2026-05-12 -- AST onto chunks: production compiler shrinks ~47%

* Saga: `ast-to-chunks` (Phase 2b of
  `docs/shrink-lgo-size.md`, four steps `pr/measure-ast` →
  `pr/ast-accessors` → `pr/ast-chunk-storage` →
  `pr/ast-baseline`).
* Rebuild signal: **REQUIRED.** The `.lgo` bytes change and the
  binary's behavior at the AST-cap boundary changes (see below).
  Downstream consumers (dcsno, dcftn) should refresh their
  installed `plsw.lgo`.
* Effect: the ten static `nd_*_arr[NODE_POOL_MAX]` parallel
  arrays in `src/ast.h` (10 × 12,288 ints = ~360 KB of
  pre-reserved `.data`) are gone. AST nodes now live in
  `struct ast_block` instances drawn from the 64 KB chunk pool
  introduced in Phase 2. The accessor macros from
  `pr/ast-accessors` (e.g. `nd_kind(i)`) make the move
  transparent at all 406 callsites.
  - `AST_NODES_PER_CHUNK = 136` (10 fields × 3 bytes × 136 =
    4,080 bytes, fits one 4 KB chunk with 16 bytes of slack).
  - `AST_CHUNKS_MAX = CHUNK_MAX = 16`. Effective node cap:
    **2,176** (vs. the prior static cap of 12,288; 5.4×
    headroom over the 406-node fixture peak recorded in
    `docs/memory-audit-2026-05-10.md`).
  - `compile_program()` calls `chunk_init()` first thing.
    `ast_init()` returns owned chunks to the pool between
    compiles; REPL-mode back-to-back compiles confirmed clean
    across 30+ cycles.
* **Cap-boundary message change.** Exhaustion now reports
  "AST pool exhausted (chunk table full)" or "chunk_alloc
  returned NULL for AST node" instead of the prior
  "AST node pool exhausted (12288 nodes)". Any code or fixture
  that greps the old message needs updating.
* Build SHAs:
  - `build/plsw.lgo` SHA-256
    `3877480056c5ded09a0891305ae2574b214aa3cef4142c65fc049917720affef`
    (872,174 bytes; **-785,256 bytes / -47.4%** vs. the
    post-`chunk-allocator` build of 1,657,430).
  - `build/plsw_test.lgo` SHA-256
    `7ffd684c01c006cff1e79ba2e04f4c9e2cc4e06400a8f8f77b2c5f6a18375a3d`
    (884,680 bytes; **-778,696 bytes / -46.8%** vs.
    1,663,376).
* No semantic regression: `just test` 15/15 green; in-binary
  emulator suites 4/5/14/17/36/37 all 0 errors; all-suite run
  confirms chunk reset hygiene. Pre-existing
  streaming-emit-contract failures on suites 31-35 are
  unchanged (verified against the pre-saga baseline; out of
  scope for this saga).
* Architecture context: `docs/shrink-lgo-size.md` Phase 2b is
  marked DONE 2026-05-12. The headline ≤ 700 KB target is
  still 172 KB out, but `plsw.lgo` at 872 KB leaves COR24's
  1 MB SRAM with enough headroom that dcsno's
  `saga-expr-completeness` and dcftn are **no longer gated**
  by compiler size. Phase 3 (`buffer-to-chunks`) is optional,
  contingent on whether dcsno/dcftn need additional headroom
  after rebuilding.
* Rebuild / install command:
  ```sh
  cd $SRCROOT
  just clean
  just build-lgo
  install -m 0640 build/plsw.lgo $TOOLROOT/../lib/cor24/plsw.lgo
  ```

## 2026-05-10 -- chunk allocator scaffolding (no semantic change)

* Infrastructure: `feat(chunk): scaffold 4KB chunk allocator`
  + `test(chunk): add test_chunk suite` + this baseline (saga
  `chunk-allocator`, Phase 2 of `docs/shrink-lgo-size.md`).
* Rebuild signal: this same saga's mark-pr (final step).
* Effect: adds `src/chunk.h` -- a header-only 4 KB-chunk
  allocator (`CHUNK_SIZE = 4096`, `CHUNK_MAX = 16`) carving a
  single 64 KB pre-reservation into chunks, with the API
  `chunk_init` / `chunk_alloc` / `chunk_free` / `chunk_used`.
  Each chunk equals the project's 4 KB static-block ceiling,
  the granularity Phase 5's lint script will enforce.
  `chunk_storage` carries `/* lint-exempt: chunk-pool */` --
  it's the only static array in the project deliberately
  exceeding 4 KB. **No callers in production** -- tc24r's DCE
  elides `chunk_init/alloc/free/used` from `build/plsw.s`. The
  static cost is `chunk_storage` (`.zero 65536`) +
  `chunk_table` (`.zero 96`).
* No semantic change to the production compiler. Downstream
  consumers (dcsno, dcftn) **do not need to rebuild** -- their
  inputs produce identical `.s` output. The reinstall here is
  about parity (so the relayed `plsw.lgo` matches the saga's
  recorded SHA), not behavior.
* Build SHA: `build/plsw.lgo` SHA-256
  `712fb0be04cf639c82b8887638c4735224f8ae34bb53426ae43ec761456372a0`
  (1,657,430 bytes; +145,848 bytes / +14% vs the post-test-split
  build of 1,511,582 -- the 64 KB pool reservation plus its
  `.lgo` encoding overhead). `build/plsw_test.lgo` SHA-256
  `b829f2862555308c849080cf7978069277e25902f608bfbec43a32295c770d88`
  (1,663,430 bytes -- includes the test_chunk suite and the
  surviving `chunk_*` function bodies).
* The actual headline shrink lands in the **next saga
  (`ast-to-chunks`)** when the 360 KB AST static pool migrates
  onto chunks. Net projected after that: ~296 KB net reclaim
  (-360 KB AST static + 64 KB pool already paid here).
* Rebuild / install command:
  ```sh
  cd $SRCROOT
  just clean
  just build-lgo
  install -m 0640 build/plsw.lgo $TOOLROOT/../lib/cor24/plsw.lgo
  ```

## 2026-05-10 -- test split: production compiler shrinks ~13%

* Refactor: `refactor(build): split tests out of main.c into
  test_main.c` (saga `pr/test-split`, Phase 1 of
  `docs/shrink-lgo-size.md`).
* Rebuild signal: this same saga's mark-pr.
* Effect: 46 `test_*` functions and the `run_suite()` dispatcher
  move from `src/main.c` (which tc24r compiles into the shipped
  `plsw.lgo`) into a new `src/test_main.c` (which builds a
  separate `plsw_test.lgo` for in-binary diagnostic suites). The
  shared `compile_program()` driver and its COR24 runtime
  preamble emitters move into `src/compile.h` so both binaries
  reuse them without duplication. `just build-lgo` keeps making
  the production compiler; new `just build-test-lgo` makes the
  test runner. `just test` (reg-rs) is unaffected -- it uses
  `pipeline.sh` which drives the production `plsw.lgo` against
  `.plsw` fixtures, never the in-binary suites.
* Build SHA: `build/plsw.lgo` SHA-256
  `f6210655b99b51b6a87799c8620c2a95f16f80a34f11c5c1a8bcbf734d2e74d7`
  (1,511,582 bytes; ~13% smaller than the 2026-05-09 streaming
  build of 1,738,122 bytes -- ~226 KB freed by removing test
  text from production).
* Direct downstream: nothing is gated on this rebuild yet.
  Phase 2 (AST chunk allocator, `docs/shrink-lgo-size.md` Phase
  2) is the next planned reduction.
* Rebuild / install command:
  ```sh
  cd $SRCROOT
  just clean
  just build-lgo
  install -m 0640 build/plsw.lgo $TOOLROOT/../lib/cor24/plsw.lgo
  ```

## 2026-05-09 -- streaming `.s` emission, 4 KB coalescer

* Codegen change: `feat(emit): stream .s emission, replace
  256 KB emit_buf with 4 KB coalescer` (saga
  `pr/streaming-emit`).
* Rebuild signal: this same saga's mark-pr.
* Effect: replaces the per-compilation `emit_buf` accumulator
  (256 KB) with a 4 KB coalescing flush buffer that drains to
  UART when full. The `.s` stream is unbounded by buffer size;
  the 256 KB ceiling that capped library-module compilation
  goes away, and PL/SW gets ~252 KB of SRAM headroom.
* Direct downstream: dcsno's `saga-expr-completeness` step 003
  unblocks. `sno_exec.s` was at 99.96% of the old ceiling
  (262,030 / 262,144 bytes); any further library-module growth
  would have overflowed.
* Side effect: `emit_output()` now reflects only the unflushed
  tail (the bytes since the last `emit_flush()`). Production
  `compile_program()` calls `emit_flush()` at end and the
  bytes have already streamed to UART via the per-buffer-fill
  flush. `main()`'s compile-mode handler dropped its
  `uart_putstr(out)` line accordingly. Tests in `src/main.c`
  that emit small fragments (< 4 KB) and inspect via
  `str_find` continue to work because no flush triggers
  mid-fragment; tests emitting > 4 KB would only see the tail
  (none currently exist that hit this).
* Build SHA: `build/plsw.lgo` SHA-256
  `b6525ebe7c2e3c0dfd073dc5fe765bf8786f3672c0adf2e85bbf4287ac081d11`
  (1,738,122 bytes; ~25% smaller than the May 9 13:35 build,
  reflecting the freed 256 KB).
* Rebuild / install command (same shape as the prior entry):
  ```sh
  cd $SRCROOT
  just clean
  just build-lgo
  install -m 0640 build/plsw.lgo $TOOLROOT/../lib/cor24/plsw.lgo
  ```

## 2026-05-09 -- `.zero N` codegen for all-zero static data

* Codegen change: `c7e1262 feat(codegen): emit .zero N for
  all-zero static data` (saga `pr/emit-zero-fill`, merged via
  `pr/emit-zero-fill` → `dev`).
* Rebuild signal: `pr/rebuild-plsw-lgo`.
* Effect: `cg_emit_static_var` collapses the per-byte
  `.byte 0,0,...,0` form for zero-init static arrays into a
  single `.zero <total_bytes>` directive. Byte-identical `.bin`
  output; the win is in `.s` source-text density. Side-effect
  bug fix: `INIT(0)` on an N-byte array used to emit only
  `.word 0` (3 bytes); now emits the correct width.
* Direct downstream: SNOBOL4's `sno_main.s` drops from ~261 KB
  to ~7 KB (97.7% of that file was enumerated zero-fill text).
  Unblocks dcsno's `pr/sno-engine-consolidation`.
* Rebuild command (from a clean checkout of `main`):
  ```sh
  cd $SRCROOT
  just clean
  just build-lgo
  # produces build/plsw.lgo (~2.3 MB)
  ```
* Install target: `cp build/plsw.lgo $TOOLROOT/../lib/cor24/plsw.lgo`
  (mike's relay action; no source change needed at install).
* Verification: after install, `pl-sw` invocations on a `.plsw`
  containing `DCL X(64) BYTE INIT(0);` should emit `.zero 64`
  in the resulting `.s`. dcsno's rebuilt `snobol4.lgo` should
  produce a `sno_main.s` under 20 KB.
* Build-this-saga reference hash: `build/plsw.lgo` SHA-256
  `6a0e65706389a22f59f86b0c398cda401a21e90c1cb46e70bb27888fb186c7fc`
  (built from `dev` at HEAD `53206b1` on 2026-05-09; mike's
  rebuild from `main` after promotion will hash differently if
  `main` carries different commits, but should be deterministic
  given identical source + identical `tc24r`/`cor24-asm`).
