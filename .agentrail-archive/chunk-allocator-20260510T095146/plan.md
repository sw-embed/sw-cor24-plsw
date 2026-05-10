# chunk-allocator (Phase 2 of shrink-lgo)

Per `docs/shrink-lgo-size.md` Phase 2. Build the chunk allocator
infrastructure that later sagas (`ast-to-chunks`, `buffer-to-chunks`)
will draw from. **No pool migration in this saga** — only the
allocator API, its tests, and a baseline measurement of the new
`.lgo` size.

## Sizing decision

Per discussion 2026-05-10, the pool is sized smaller than both the
brief and the shrink-lgo doc originally proposed:

* `CHUNK_SIZE = 4096` (chars; matches the 4 KB static-block ceiling
  rule from Phase 5 — every chunk is exactly one ceiling-unit).
* `CHUNK_MAX = 16` (16 × 4 KB = **64 KB total pre-reserved pool**).

Rationale: 64 KB is the entire dynamic-memory budget for the
production compiler. Small enough that Phase 2 doesn't blow past
the 1 MB SRAM ceiling on its own; granular enough that pools
(AST, arena, etc.) can request just what they need. If Phase 0
measurements during `ast-to-chunks` show this is too tight, bump
`CHUNK_MAX` then — measurement-driven, not aspirational.

## Approach

* Project convention is header-only modules (see `arena.h`,
  `ast.h`, etc. — only `main.c` and `test_main.c` are `.c` files).
  So implementation lives in `src/chunk.h`, included by both
  `main.c` and `test_main.c`.
* Static state:
  - `struct chunk_desc { int in_use; char *base; }` × `CHUNK_MAX`
  - `char chunk_storage[CHUNK_MAX * CHUNK_SIZE]` — the only
    static block in the project that exceeds the 4 KB ceiling;
    carries `/* lint-exempt: chunk-pool */` marker for Phase 5.
* API:
  - `void chunk_init(void)` — mark all chunks free, fill `base`
    pointers from `chunk_storage`.
  - `char *chunk_alloc(void)` — return base of a free chunk, or
    `NULL` when exhausted.
  - `void chunk_free(char *base)` — mark the chunk owning `base`
    free. Silent no-op on an unknown pointer (don't trap — keep
    the freestanding contract simple).
  - `int chunk_used(void)` — count of in-use chunks (for budget
    reporting; consumed by Phase 0-style measurement code later).

## Tests

Add `test_chunk_*` to `src/test_main.c` and a new entry in
`run_suite()`:

1. `test_chunk_init` — after `chunk_init()`, `chunk_used() == 0`.
2. `test_chunk_alloc_distinct` — `CHUNK_MAX` allocs return
   non-NULL, distinct, page-aligned `base` pointers; `chunk_used()
   == CHUNK_MAX`.
3. `test_chunk_exhaustion` — the `(CHUNK_MAX+1)`th alloc returns
   `NULL`; `chunk_used()` unchanged.
4. `test_chunk_free_reuse` — free a chunk, alloc again, get the
   freed `base` back; `chunk_used()` decrements then increments.
5. `test_chunk_free_unknown` — `chunk_free(NULL)` and
   `chunk_free(some_random_ptr)` are no-ops (state unchanged).

Run via `build/plsw_test.lgo` for hands-on verification. Reg-rs
(`just test`) is unaffected — production compile path doesn't
touch chunk_* yet — so 15/15 must stay green.

## Steps

1. **chunk-api**. Create `src/chunk.h` with the data structures,
   API, and `lint-exempt: chunk-pool` marker. `#include "chunk.h"`
   from both `main.c` and `test_main.c`. Build both `.lgo`s clean
   (no callers yet — just verifies the header compiles in both
   contexts and the static block lands in the binary).

2. **chunk-tests**. Add the five `test_chunk_*` functions to
   `src/test_main.c` and a `run_chunk_suite()` (or an entry in
   `run_suite`). Verify `plsw_test.lgo` runs the new tests
   successfully under `cor24-run`. Confirm `just test` reg-rs
   suite stays 15/15 green.

3. **chunk-baseline**. Capture new SHA + size for `plsw.lgo`
   (will grow ~64 KB from the pre-reservation — expected and
   recorded as the Phase 2 cost). Capture new size for
   `plsw_test.lgo`. Update `docs/shrink-lgo-size.md` Phase 2
   section to "DONE 2026-05-10" with measured numbers and the
   final API. Add CHANGES.md rebuild-trigger entry. Sets the
   stage for the next saga (`ast-to-chunks`).

## What does NOT go in this saga

* No pool migration (AST stays in its 10 parallel arrays;
  arena_buf stays static; src_buf stays static). All migrations
  are subsequent sagas.
* No lint script (Phase 5, separate saga).
* No tc24r change.
* No measurement-driven `CHUNK_MAX` retuning — that's Phase 0's
  job inside `ast-to-chunks` once we can actually count consumed
  chunks against real compiles.
