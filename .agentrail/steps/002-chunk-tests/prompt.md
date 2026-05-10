Add `test_chunk_*` functions to `src/test_main.c` exercising the
chunk allocator API from `src/chunk.h`. Run them via the test
binary and verify they pass.

Tests to add (exactly five, each a `void test_chunk_*(void)`
following the existing `test_*` pattern in `src/test_main.c`):

1. `test_chunk_init` — call `chunk_init()`, assert
   `chunk_used() == 0`. Re-call `chunk_init()` after some allocs;
   assert `chunk_used() == 0` again.

2. `test_chunk_alloc_distinct` — `chunk_init()`; allocate
   `CHUNK_MAX` chunks; assert each return is non-NULL and that all
   `CHUNK_MAX` returned `base` pointers are pairwise distinct.
   Assert `chunk_used() == CHUNK_MAX`. (No alignment assertion —
   layout is tc24r's call.)

3. `test_chunk_exhaustion` — `chunk_init()`; allocate `CHUNK_MAX`
   chunks; the next `chunk_alloc()` returns `NULL`;
   `chunk_used() == CHUNK_MAX` (unchanged).

4. `test_chunk_free_reuse` — `chunk_init()`; allocate `CHUNK_MAX`
   chunks (capture pointers); `chunk_free(p3)`; assert
   `chunk_used() == CHUNK_MAX - 1`; `chunk_alloc()` returns `p3`
   (the freed slot, since it's the only free one); `chunk_used() ==
   CHUNK_MAX` again.

5. `test_chunk_free_unknown` — `chunk_init()`; allocate one chunk;
   `chunk_free(NULL)` and `chunk_free((char *)0xDEAD)` are both
   no-ops; `chunk_used() == 1` (unchanged).

Wire the tests into `run_suite()` (or its equivalent dispatcher in
`test_main.c`) so they run alongside the existing 46 suites. Add a
`uart_putstr` banner per test, matching the project's existing
test output style.

Verification:

1. `just build-test-lgo` succeeds.
2. Run `plsw_test.lgo` under `cor24-run` for the chunk suite
   specifically; all five tests print PASS (or however the
   existing tests signal success — match the convention).
3. `just test` (reg-rs) — must remain 15/15 green. Production
   path is untouched, so this is a smoke check, not the primary
   signal.

Commit on `feat/chunk-tests` (after `dg-new-feature chunk-tests`).
Include `.agentrail/` files in the same commit. Stop at commit.
`agentrail complete --summary "..." --reward 1 --actions "..."`,
then `dg-mark-pr`, then STOP.

## Out of scope

* Changing the allocator implementation in `src/chunk.h` (only fix
  bugs revealed by the tests; don't add features). If tests reveal
  a real bug, fixing it is in scope for this step; expanding the
  API is not.
* Migrating any existing pool to use `chunk_*`.
* Documentation updates (final step's job).
