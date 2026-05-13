# capacity-and-test (combined pool/buffer fix)

Two coupled deliverables, addressing two dcpls-owned briefs filed
2026-05-12. They share a `.lgo` size budget, so they ship together
rather than as separate sagas.

## Background

* **Capacity regression in shipped `pr/ast-chunk-storage`** (mike's
  `dcpls-ast-chunk-capacity-and-test.md`, 2026-05-12 17:01). The
  Phase 3 migration replaced a 332 KB static AST pool with
  `CHUNK_MAX=16 × CHUNK_SIZE=4096 = 64 KB` of chunk-backed AST
  storage — ~1/5 the capacity. Real workloads (`sno_lex.plsw`)
  exit with `ERROR: AST pool exhausted (chunk table full)`. The
  saga's step 1 (`measure-ast`) only profiled the 15 reg-rs
  fixtures, peak 406 nodes, which fit easily and hid the real
  problem. Production `work/lib/cor24/plsw.lgo` is rolled back to
  the pre-Phase-3 build (1,657,430 bytes) until this is fixed.
* **SRC_BUF overflow** (dcsno's `dcpls-enlarge-src-buf.md`,
  2026-05-12 15:01). The consolidated `sno_engine.plsw`
  (137,994 bytes) exceeds `SRC_BUF_SIZE = 65536` and trips the
  new overflow detector (`ERROR: source read failed`). Brief asks
  for 192 KiB (interim) or 256 KiB (recommended); user has
  chosen the **192 KiB interim** to keep the static cost
  predictable while the capacity fix is in flight.

The two bumps add static cost to `plsw.lgo`. Bundling them lets
us account for the total size honestly in one CHANGES.md entry
and one `dg-mark-pr` cycle rather than sequencing two installs.

## Approach (3 steps)

### 1. measure-real-inputs

Re-apply the `#ifdef MEASURE` instrumentation that step
`001-measure-ast` used (now archived under
`.agentrail-archive/ast-to-chunks-20260512T170341/`). Extend it
to also track peak `ast_chunk_count` so the new sizing decision is
data-driven, not guessed.

Targets to measure:

- `sno_lex.plsw` (60 KB, 1535 lines) and `sno_exec.plsw` (63 KB)
  from `~/github/sw-embed/sw-cor24-snobol4` via the readable
  sibling clone at
  `/disk1/.../work/dcsno/github/sw-embed/sw-cor24-snobol4`.
  Drive the instrumented `plsw.lgo` with the same FILE:/SOURCE:
  framing that `sw-cor24-snobol4/scripts/build.sh` uses.
- `sno_engine.plsw` if reachable (consolidated 137 KB file; this
  is the SRC_BUF tripwire). May be on a non-merged dcsno feat
  branch.
- All 15 reg-rs fixtures (carry-over from step
  001-measure-ast). They should still pass; what matters is that
  the worst-case fixture is now the new sno_lex peak, not
  storage_coalesce's 406.

Captured numbers go into `docs/memory-audit-2026-05-12.md`
(new file; the May 10 audit stays untouched as the prior
baseline). Decision recorded: target `CHUNK_MAX` value (or
`CHUNK_SIZE` increase) sized to **measured peak × 1.5** with a
note on the lower bound being "≥ pre-Phase-3's 332 KB so we
don't regress capacity again".

Revert the instrumentation at the end of the step (same pattern
as 001-measure-ast). Commit. No `pr/` rename yet -- the work
isn't ready to install.

### 2. apply-bumps-and-stress-test

Two bumps + a regression test.

**Chunk capacity bump.** Update `src/chunk.h` and/or `src/ast.h`
to the step-1-derived values. Two-axis choice:

- Keep `CHUNK_SIZE = 4096` (preserves the 4 KB static-block
  ceiling discipline from Phase 5 lint) and grow `CHUNK_MAX` to
  ~128 -> ~512 KB pool, OR
- Grow `CHUNK_SIZE` to 16-32 KB with fewer chunks. Pool stays
  ~512 KB. **Tradeoff:** larger chunk size needs the chunk
  storage to remain `/* lint-exempt: chunk-pool */` (already
  done) and the AST_NODES_PER_CHUNK math must be re-derived
  (10 fields × 3 bytes × N <= CHUNK_SIZE → N up to ~1364 for
  CHUNK_SIZE=4096... wait actually that's 1365 not 136). The
  step-3 baseline must reconcile the prior 136 figure with the
  new layout if CHUNK_SIZE changes.

Recommendation (revisit after step 1 numbers): keep
`CHUNK_SIZE=4096` and bump `CHUNK_MAX`. Smallest blast radius;
preserves the lint story.

**SRC_BUF bump.** `src/main.c`: `SRC_BUF_SIZE` from `65536` to
`196608` (192 × 1024). The brief's interim recommendation.

**Stress regression test.** Add a fixture under
`tests/inputs/large/<stem>.plsw` that exceeds the historical
peak by a documented margin (e.g. 1.5× the sno_lex peak from
step 1). The fixture is a `.plsw` program — synthetic but
realistic in shape (many DCLs, several procs, a few hundred
expressions). Wire into `just test` via either:

- A new `reg-rs/plsw_chunk-stress.{rgt,out,err}` entry that
  drives the production `plsw.lgo` against the fixture and
  asserts the `.s` is produced without `AST pool exhausted` /
  `SYNTAX ERROR` / `STORAGE ERROR` markers in stderr; or
- A `scripts/test.sh` extension that calls the fixture
  explicitly.

The first form is preferable -- it lives in the same harness as
the existing 15 fixtures and contributes a 16th to the count.

Optionally: have `compile_program()` print a one-line
`chunks_used=N/CHUNK_MAX` summary at end-of-compile (gated on a
`%DEFINE CHUNK_PROFILE` or always-on; small constant cost).
Step 1's numbers should inform whether this is worth carrying
in production.

Build + verify:

- `just clean && just build-lgo` -- production succeeds.
- `just build-test-lgo` -- test runner succeeds.
- `just test` -- reg-rs is now 16/16 green (15 prior fixtures
  + 1 stress fixture). The stress fixture must drive the
  pool deep enough that the old `CHUNK_MAX=16` would have
  failed it; document that fact in the fixture comment.
- A spot-check compile of `sno_lex.plsw` end-to-end. Captures
  the original real-workload failure mode and proves the bump
  resolves it.

Commit. No `pr/` rename yet.

### 3. baseline

Capture post-fix sizes + close the saga.

- `just clean && just build-lgo && just build-test-lgo`.
- `wc -c` + `sha256sum` for both `.lgo` files.
- Compare against three earlier datapoints:
  - **Pre-Phase-3** (the rolled-back production):
    1,657,430 bytes.
  - **Broken Phase-3** (current main, not installed):
    872,174 bytes.
  - **This fix:** expected somewhere between, depending on the
    chunk bump.

Update `docs/shrink-lgo-size.md`:

- Add a Status (2026-05-12 part 2) section noting the
  capacity regression was found and fixed, the new
  CHUNK_MAX/CHUNK_SIZE values, and the new measured peak.
- Update Phase 2b's DONE block: replace the now-stale "5.4×
  headroom" wording with the new headroom over the
  step-1 peak.
- Note the new SRC_BUF_SIZE under the buffers table or as a
  separate small entry.

Update `CHANGES.md`:

- New 2026-05-12 entry below the existing one. Mark **Rebuild
  signal: REQUIRED.** Replaces the prior "rebuild required"
  entry as the latest install signal -- the prior 872-KB
  binary should not be installed.
- Record both bumps (chunk capacity + SRC_BUF), the new
  SHA, size, and the post-fix headroom.

`agentrail complete --done` to close the saga.
`dg-mark-pr` and STOP.

## Out of scope

* **Phase 4 buffer-to-chunks**: stays for a future saga.
* **Static `_MAX` audit (Phase 5)**: not pre-empted here.
* **Performance benchmark harness**: pass/fail gate only; no
  cycle counts.
* **Reverting `pr/ast-chunk-storage` on `main`**: rollback is
  install-side only; the Phase-3 code stays.
* **`INC_BUF_SIZE` bump**: the dcsno brief explicitly says
  leave alone; macro+include load is well under 32 KB.
* **The streaming-emit failure on plsw_test suites 31-35**: still
  out-of-scope (it's a pre-existing test-harness bug in
  `compile_program`'s emit_output contract; verified pre-saga
  baseline has it too).

## Risks

* **Sibling-clone read access.** Step 1 reads
  `sw-cor24-snobol4` source from another agent's clone
  (`work/dcsno/...`). Confirmed readable by group (mode 660,
  group devgroup) on 2026-05-12. If permissions change, fall
  back to the synthetic stress fixture from step 2 for the
  measurement -- it's the durable artifact anyway.
* **tc24r macro rescan depth ≤ 2.** Project memory note from
  the just-finished saga. Don't introduce new triple-nested
  function-like macro calls when adding any new accessors.
* **CHUNK_SIZE > 4096 path.** If we go that route, the lint
  discipline still allows it (chunk_storage is already
  lint-exempt) but the `AST_NODES_PER_CHUNK` math in
  `src/ast.h` and any doc references need re-derivation. The
  136 figure is wired into the Phase 2b doc and the macros.
* **plsw.lgo size growth.** Likely from 872 KB toward
  ~1.0-1.2 MB depending on chunk bump. Still well under the
  pre-Phase-3 1.66 MB. Document honestly in step 3 -- this
  walks back some of Phase 2b's win, but the win was
  conditional on correctness.
