Capture post-Phase-2b baseline measurements and close the
ast-to-chunks saga. This is the saga whose net effect unblocks
dcsno's saga-expr-completeness step 003 and the downstream dcftn
work.

Steps:

1. `just clean && just build-lgo && just build-test-lgo`.

2. Capture sizes + SHAs:
   - `wc -c build/plsw.lgo` and `sha256sum build/plsw.lgo`.
   - `wc -c build/plsw_test.lgo` and `sha256sum build/plsw_test.lgo`.

3. Calculate the delta from the previous baseline (post-chunk-
   allocator: 1,657,430 bytes for plsw.lgo). Expected: drop to
   ~1.36-1.40 MB, a ~250-300 KB net shrink (= 360 KB AST static
   reclaimed minus the ~64 KB pool already paid).

4. Update `docs/shrink-lgo-size.md`:
   - Mark Phase 2b ("Migrate the AST onto chunks") with
     `-- DONE 2026-05-10` (matching prior phases' style).
   - Insert measured numbers under Phase 2b: the new plsw.lgo
     size, the delta, peak chunk usage from end-of-compile, the
     final AST_NODES_PER_CHUNK and AST_CHUNKS_MAX values.
   - Update the saga sequencing list at the bottom: mark
     ast-to-chunks DONE.
   - Add a "dcsno/dcftn unblock" note at the top under the
     existing "gating" framing -- explicitly state whether the
     headline target is reached and whether downstream sagas can
     now restart.

5. Add a CHANGES.md entry recording:
   - The new plsw.lgo SHA + size + delta.
   - The new plsw_test.lgo SHA + size + delta.
   - **Rebuild signal: REQUIRED** for downstream consumers --
     the binary's behavior at the AST-cap boundary changes
     (chunk-overflow message replaces pool-overflow message),
     and the .lgo bytes are different. dcsno/dcftn should
     refresh their installed plsw.lgo.
   - Pointer to docs/shrink-lgo-size.md Phase 2b section for
     the architecture context.

6. Optional: run `just test` one more time and capture the
   reg-rs result + count in the CHANGES.md entry to demonstrate
   no semantic regression.

7. Commit on `feat/ast-baseline`. Include `.agentrail/`.

8. `agentrail complete --done --summary "..." --reward 1
   --actions "..."`. The `--done` flag closes the saga.

9. `dg-mark-pr` and STOP.

## Out of scope

* Starting the next saga (buffer-to-chunks if needed, or
  static-lint, or shrink-lgo-verify). Each is its own saga,
  decided after this one's effect is measured against
  dcsno/dcftn's actual blockers.
