Re-do Phase 0 measurement against realistic inputs to size the
chunk pool correctly. The original `001-measure-ast` step only
profiled the 15 reg-rs fixtures (peak 406 nodes); that was not
representative and the resulting `CHUNK_MAX=16 × 4 KB = 64 KB`
pool is now causing real-workload failures.

Steps:

1. Re-apply `#ifdef MEASURE` instrumentation to `nd_alloc()` in
   `src/ast.h`. Track two peaks:
   - `peak_nd` (the original counter -- still useful as a cap
     value).
   - `peak_chunks` (= max `ast_chunk_count` observed during
     compilation; = ceil(peak_nd / AST_NODES_PER_CHUNK)).
   Emit both on end-of-compile, e.g. via a one-liner in
   `compile_program()` printed only when `MEASURE` is defined:
   `uart_putstr("AST peak: <peak_nd> nodes, <peak_chunks>
   chunks\n");`.

2. Build an instrumented production binary:
   `tc24r src/main.c -o build/plsw-meas.s -DMEASURE -I ...`
   then `cor24-asm build/plsw-meas.s -o build/plsw-meas.lgo`.

3. Run the instrumented binary against:
   a. **sno_lex.plsw** -- the realistic largest known input. Path:
      `/disk1/.../work/dcsno/github/sw-embed/sw-cor24-snobol4/src/sno_lex.plsw`.
      Drive via FILE:/SOURCE: framing (snobol4's
      `scripts/build.sh` is the reference). Macros to register:
      `include/descr.msw`, `include/heap.msw`, `include/am.msw`,
      `include/pat.msw`, `include/snoglob.msw`. Library .plsw to
      include: `src/sno_util.plsw` (it's a dependency per the
      file header).
   b. **sno_exec.plsw** -- also a realistic large input.
   c. **sno_engine.plsw** -- the 137 KB consolidated module IF
      reachable from any dcsno feat branch (likely
      `feat/runtime-split-resume` per the enlarge-src-buf
      brief). If unreachable, skip and note in the audit.
   d. **All 15 reg-rs fixtures** -- carry-over from
      001-measure-ast. Run via reg-rs's normal driver but with
      the instrumented binary. Captures the post-saga state of
      the worst-case test fixture.

4. Capture peak numbers in
   `docs/memory-audit-2026-05-12.md` (new file; the May 10
   audit stays put as the prior baseline). Required content:
   - Per-input peak_nd + peak_chunks table.
   - The overall maximum (this is the sizing input for step 2).
   - Decision: target `CHUNK_MAX` (and/or `CHUNK_SIZE`) such
     that capacity is **>= peak * 1.5** AND **>= 332 KB**
     (the pre-Phase-3 static pool, so we don't structurally
     regress capacity again).
   - Note whether `sno_engine.plsw` was reachable; if not, log
     a follow-up.

5. Revert the instrumentation (same pattern as
   `001-measure-ast`: instrument, measure, revert -- production
   source stays clean). The instrumented binary is throwaway;
   step 2 will produce the actual fix.

6. Sanity-check: `just clean && just build-lgo` -- production
   builds clean with no MEASURE traces left in src/. `just test`
   -- reg-rs 15/15 green.

7. Commit on `feat/capacity-and-test`. Include the new
   `docs/memory-audit-2026-05-12.md` and any `.agentrail/`
   updates. Stop at commit (no push, no pr/ rename -- this is
   step 1 of 3).

8. `agentrail complete --summary "..." --reward 1
   --actions "..."`. Do NOT --done (saga continues to step 2).

## Risks

* sibling-clone read access to the dcsno tree might be off if
  permissions change. Fallback: if sno_lex.plsw isn't readable,
  fabricate a synthetic stress program that approximates its
  node count (build with a script that emits many DCLs +
  procs); use it as the measurement target. The synthetic
  fixture will become step 2's regression test anyway.
* The instrumented binary takes longer to compile sno_lex than
  the reg-rs fixtures (60 KB vs ~1 KB). Use generous
  `-n 1000000000 -t 600 --speed 0` flags for cor24-emu.

## Out of scope

* Applying the bump. Step 2 does that.
* The SRC_BUF bump. Independent of measurement; step 2.
* Adding the regression test. Step 2.