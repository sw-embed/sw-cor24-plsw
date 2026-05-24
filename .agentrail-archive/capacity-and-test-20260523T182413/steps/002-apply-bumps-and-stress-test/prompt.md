Apply both capacity bumps and add the regression test that would
have caught the Phase 3 sizing miss. Builds on step 1's measured
peaks (in `docs/memory-audit-2026-05-12.md`).

Steps:

1. **Chunk capacity bump.** Update `src/chunk.h` (and any
   constants that mirror it in `src/ast.h`). Default
   recommendation: keep `CHUNK_SIZE = 4096` (preserves the
   project's 4 KB static-block lint ceiling) and bump
   `CHUNK_MAX` to the value chosen in step 1. Sanity-check the
   chosen value against:
   - **>= peak from step 1 × 1.5** (headroom for growth).
   - **>= 332 KB total pool** (so we don't regress capacity
     vs. the pre-Phase-3 static AST allocation).
   - **lint-exempt note** stays on `chunk_storage`.
   If step 1's measurement said CHUNK_SIZE should grow
   instead (e.g. peak chunks > 64 would push CHUNK_MAX
   uncomfortably high), do that and re-derive
   `AST_NODES_PER_CHUNK` in `src/ast.h`. The macros
   `nd_kind(i)` etc. don't care about the specific N, but the
   doc/code references to "136 nodes per chunk" need updating.

2. **SRC_BUF bump.** `src/main.c`: change `SRC_BUF_SIZE` from
   `65536` to `196608` (192 × 1024, the brief's interim
   value). This unblocks dcsno's `sno_engine.plsw`
   consolidation (137 KB single source). Keep the
   overflow-detection error path untouched (it stays useful
   as a tripwire if growth ever needs the next bump).

3. **Stress regression test fixture.** Create
   `reg-rs/plsw_chunk-stress.rgt` (and the matching `.out` /
   `.err` files via `just test-bootstrap-goldens` after the
   fixture compiles cleanly). The fixture must:
   - Be a single `.plsw` program large enough to drive the
     AST pool well past the old `CHUNK_MAX=16` limit. Aim
     for ~1.5× the sno_lex peak from step 1.
   - Be synthetic but realistic-shaped: many DCLs (mix of
     scalar / array / record), several PROCs (some recursive),
     a few hundred expression statements. Avoid pathological
     macro use -- the test is about the AST pool, not the
     macro engine.
   - Carry a comment at the top explaining its purpose and
     citing this saga + `dcpls-ast-chunk-capacity-and-test.md`.
   - Document the expected peak (step 1's number × 1.5) in the
     comment so future maintainers can see why it's sized
     this way.

   Wire it into the existing reg-rs harness (it should pick up
   `plsw_*.rgt` automatically per `scripts/test.sh` and the
   `tests/driver.sh` convention). After bootstrap, the
   `.out` is the expected `.s` produced by today's bumped
   compiler.

4. **Optional chunk-peak telemetry.** Add a small end-of-compile
   summary to `compile_program()` printed under a `%DEFINE
   CHUNK_PROFILE` guard (compile-time gate, not a runtime cost
   for production). One line:
   `chunk_peak=<n>/<CHUNK_MAX>`. Useful for future debugging
   without permanently inflating the production output.
   Skip if the addition risks the streaming-emit-contract
   pre-existing bug (uart_putstr after compile_program's
   streaming emits).

5. **Spot-check on the real-workload failure.** Compile
   sno_lex.plsw end-to-end with the bumped `plsw.lgo`. Must
   succeed. Capture the produced `.s` line count (or first 20
   / last 20 lines) for the step summary so the unblock is
   visible.

6. Build + test:
   - `just clean && just build-lgo`. Production lgo builds.
   - `wc -c build/plsw.lgo` -- capture size; will be reported
     in step 3's CHANGES entry.
   - `just build-test-lgo`. Test runner builds.
   - `just test` -- reg-rs is now **16/16 green** (15 prior
     fixtures + 1 stress fixture).
   - Optional: emulator-run plsw_test suites 4 (AST), 37
     (chunk), 14 (codegen) to spot-check no semantic
     regression.

7. Commit on `feat/capacity-and-test`. Include the fixture,
   the bumped constants, the doc references that change, and
   any `.agentrail/` updates. No `pr/` rename yet.

8. `agentrail complete --summary "..." --reward 1
   --actions "..."`. Do NOT --done (saga continues to step 3).

## Risks

* The stress fixture's `.out` is golden by definition --
  anyone touching codegen will need to re-bootstrap it. Make
  sure the fixture exercises the AST pool primarily, not
  codegen surface area, so it's a stable golden.
* tc24r macro rescan depth caps at 2 (project memory). If
  any new accessor patterns appear during this step, keep
  function-like macro nesting <= 2.
* If chunk_peak telemetry uses uart_putstr after the streaming
  emit_flush, it'll hit the same pre-existing streaming-emit
  contract bug as the plsw_test 31-35 suites. Print it
  BEFORE emit_flush (or skip telemetry this saga).

## Out of scope

* Phase 4 buffer migration.
* Removing NODE_POOL_MAX define (still deprecated-with-pointer).
* Static _MAX lint script (Phase 5).