Capture the post-Phase-2 baseline measurements and update the
shrink-lgo planning doc. This closes out the chunk-allocator
saga and stages the next saga (`ast-to-chunks`).

Steps:

1. `just clean && just build-lgo && just build-test-lgo`.

2. Capture sizes and SHAs:
   - `wc -c build/plsw.lgo` and `sha256sum build/plsw.lgo`.
   - `wc -c build/plsw_test.lgo` and `sha256sum build/plsw_test.lgo`.
   - `wc -l build/plsw.s` (total `.word` count for the new
     `chunk_storage` block — should be ~21,846 lines, since
     65,536 chars / 3 = 21,845.33).

3. Update `docs/shrink-lgo-size.md`:
   - Mark Phase 2 ("AST chunk allocator") header with
     `— DONE 2026-05-10` (matching the Phase 1 style).
   - Insert measured numbers under Phase 2: production size delta
     (likely +~64 KB from the pool reservation), test size delta,
     final API summary (4 functions, sizing constants, lint-exempt
     marker present), and a one-line "what this step does NOT do"
     reminder (no migration yet).
   - Update the saga sequencing list at the bottom of the doc to
     reflect chunk-allocator status.

4. Add a CHANGES.md entry recording the new `plsw.lgo` SHA + size,
   the test binary's SHA + size, and a short rebuild-trigger note
   ("chunk allocator infrastructure added; static pool grew by
   ~64 KB; no callers yet, so no semantic change for downstream
   consumers — they do NOT need to rebuild"). Match the existing
   CHANGES.md tone and format.

5. Commit on `feat/chunk-baseline` (after `dg-new-feature
   chunk-baseline`). Include `.agentrail/` files in the same
   commit.

6. `agentrail complete --summary "..." --reward 1 --done
   --actions "..."` — the `--done` flag closes the saga.

7. `dg-mark-pr`, then STOP.

## Out of scope

* Starting the next saga (`ast-to-chunks`). Leave that to the
  next session — `agentrail next` will report no current step,
  and the next agent should ask the user (or the user should
  invoke `agentrail init ast-to-chunks` with a fresh plan).
* Re-tuning `CHUNK_MAX` based on reasoning. Tuning is
  measurement-driven, and we have no real consumers to measure
  against yet.
