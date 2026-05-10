# Memory audit -- AST peak usage (2026-05-10)

Phase 0 measurement for the `ast-to-chunks` saga. Per
`docs/shrink-lgo-size.md` Phase 2b, the AST node pool is moving
from 10 × 12,288-slot static parallel arrays to chunk-backed
storage drawn from the 64 KB pool that `chunk-allocator` shipped.
With `CHUNK_SIZE = 4096` chars and ~30 bytes per node (10 fields
× 3 bytes), one chunk holds **~136 nodes**, and the 16-chunk
budget caps at **~2,176 nodes**. This audit confirms whether
that cap is workable against representative `.plsw` inputs.

## Method

* Built `build/plsw.lgo` with transient instrumentation in
  `src/ast.h` (`int peak_nd; if (nd_count > peak_nd) peak_nd =
  nd_count;` inside `nd_alloc`, reset in `ast_init`) and
  `src/main.c` (a `uart_putstr("AST peak: "); print_int(peak_nd)`
  line emitted right after `--- end assembly ---`).
* Ran the instrumented compiler against every `.plsw` fixture
  used by `just test` (15 cases) via a small helper that mirrors
  `scripts/pipeline.sh`'s compile step but skips assemble + run.
* Captured `peak_nd` per fixture.
* Reverted the instrumentation so production source is unchanged.
  This audit doc is the durable artifact, not the
  instrumentation.

## Numbers

| Fixture | Peak `nd_count` | % of `NODE_POOL_MAX` (12288) | % of 2,176-node 16-chunk cap |
|---|---:|---:|---:|
| hello                  |   8 |  0.07% |  0.4% |
| hello_macro            |  15 |  0.12% |  0.7% |
| macro                  |  22 |  0.18% |  1.0% |
| select_demo            |  34 |  0.28% |  1.6% |
| led                    |  57 |  0.46% |  2.6% |
| define                 |  70 |  0.57% |  3.2% |
| loop                   |  70 |  0.57% |  3.2% |
| chain                  | 154 |  1.25% |  7.1% |
| record                 | 159 |  1.29% |  7.3% |
| select_nested          | 175 |  1.42% |  8.0% |
| storage_basic          | 343 |  2.79% | 15.8% |
| storage_double_free    | 350 |  2.85% | 16.1% |
| storage_size_mismatch  | 350 |  2.85% | 16.1% |
| storage_oom            | 353 |  2.87% | 16.2% |
| **storage_coalesce**   | **406** | **3.30%** | **18.7%** |

Max peak: **406 nodes (storage_coalesce)**.

## Decision

**Keep `CHUNK_MAX = 16`.** The largest fixture uses 18.7% of the
2,176-node ceiling -- 5.4× headroom. The static `NODE_POOL_MAX
= 12288` cap was 30× oversized for these workloads.

Caveats:

* The 15-fixture suite is the in-repo coverage. The downstream
  inputs that motivated this work (dcsno's expression-completeness
  fixtures, dcftn) aren't measurable from here. If `dcsno` or
  `dcftn` exercises the chunked compiler and overflows, bump
  `CHUNK_MAX` in `src/chunk.h` -- the current 64 KB pool would
  grow to 128 KB at `CHUNK_MAX = 32`, still fits the 1 MB SRAM
  ceiling with ample margin.
* Self-compile (PL/SW compiling itself) is not measurable: the
  PL/SW compiler is written in C, not PL/SW, so it doesn't
  self-host today.
* Peak is conservative: the bump allocator never frees
  intra-compile, so `peak_nd == final nd_count`, and chunk count
  needed is exactly `ceil(peak_nd / 136)`. Worst observed: `ceil(
  406 / 136) = 3` chunks. Rest of the 16-chunk budget is
  available for `arena_buf` migration (Phase 3 / `buffer-to-
  chunks` if needed) or just headroom.

## What this means for the next steps

* `ast-accessors` and `ast-chunk-storage` proceed against
  `CHUNK_MAX = 16` as currently set in `src/chunk.h`. No bump
  required at this step.
* The `ast-baseline` step's CHANGES.md entry should explicitly
  mention "5× headroom against measured peak" so future work
  doesn't relitigate the cap unnecessarily.
* If a future saga (likely `buffer-to-chunks`) wants to migrate
  `arena_buf` (24 KB static today) onto chunks too, it has
  ~13 chunks (≈ 53 KB) of pool budget left after worst-case AST
  use of 3 chunks.
