# Changes

Notable PL/SW changes that affect the shipped `plsw.lgo`. Not a
full changelog -- only entries that signal "rebuild and reinstall
the compiler" are recorded here.

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
