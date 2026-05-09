# Changes

Notable PL/SW changes that affect the shipped `plsw.lgo`. Not a
full changelog -- only entries that signal "rebuild and reinstall
the compiler" are recorded here.

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
