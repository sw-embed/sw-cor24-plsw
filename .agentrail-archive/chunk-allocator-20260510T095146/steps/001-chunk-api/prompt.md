Create the chunk allocator header `src/chunk.h` and wire it into
both binaries. **No callers; no migration.** This step lays static
infrastructure only.

Per `.agentrail/plan.md`:

1. Write `src/chunk.h` (header-only, matching project convention).
   - `#define CHUNK_SIZE 4096` (chars; equals the 4 KB ceiling).
   - `#define CHUNK_MAX  16` (16 × 4 KB = 64 KB total pool).
   - `struct chunk_desc { int in_use; char *base; };`
   - `static struct chunk_desc chunk_table[CHUNK_MAX];`
   - `static char chunk_storage[CHUNK_MAX * CHUNK_SIZE];  /* lint-exempt: chunk-pool */`
   - Declare (and either inline-define or static-define) the four
     functions: `chunk_init`, `chunk_alloc`, `chunk_free`,
     `chunk_used`. Default to static inline implementations in the
     header so any consumer that `#include`s gets the impl, matching
     the pattern in `arena.h` / `symtab.h`.

2. `#include "chunk.h"` in `src/main.c` and `src/test_main.c`.

3. Build both binaries:
   - `just build-lgo` (production) — must succeed.
   - `just build-test-lgo` (test) — must succeed.

4. Quick sanity check that `chunk_storage` actually appears in
   `build/plsw.s` (one big `.word` run after the new declaration).
   No need to assert exact size — just that it's present.

5. Commit on `feat/chunk-api` with a message describing the
   scaffolding-only nature of the step. Include any `.agentrail/`
   files touched. Stop at commit (no push).

6. `agentrail complete --summary "..." --reward 1
   --actions "..."` — describe what landed and what is deliberately
   deferred (no callers, no migration). Then `dg-mark-pr` and
   STOP.

## Out of scope for this step

* Wiring `chunk_*` into any existing pool (AST, arena, buffers).
* Tests (next step: `chunk-tests`).
* Updating `docs/shrink-lgo-size.md` (final step: `chunk-baseline`).
* Any size measurement claims (final step's job).
