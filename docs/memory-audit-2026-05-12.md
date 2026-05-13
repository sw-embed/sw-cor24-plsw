# Memory audit -- AST peak usage, Phase 2b correction (2026-05-12)

Re-measurement for the `capacity-and-test` saga. The original
Phase 0 measurement on 2026-05-10 (see
`docs/memory-audit-2026-05-10.md`) profiled only the 15 reg-rs
fixtures, found a 406-node peak (`storage_coalesce`), and
greenlit `CHUNK_MAX = 16` with a claimed "5.4x headroom". That
sizing shipped in `pr/ast-chunk-storage` (commit `8198065`,
merged 2026-05-12) and immediately broke real downstream
workloads -- dcsno's `sno_lex.plsw` / `sno_exec.plsw` exhaust
the 16-chunk pool with `ERROR: AST pool exhausted (chunk table
full)`.

This audit re-runs Phase 0 with the realistic inputs that should
have been measured the first time.

## Method

* Took the current `feat/capacity-and-test` AST/chunk code and
  added transient instrumentation:
  - `int peak_nd; int peak_chunks;` globals in `src/ast.h`.
  - Reset both in `ast_init`.
  - Update in `nd_alloc` after `nd_count` increments:
    `if (nd_count > peak_nd) peak_nd = nd_count;` and
    `if (ast_chunk_count > peak_chunks) peak_chunks = ast_chunk_count;`.
  - One-line print in `src/main.c`'s compile-mode handler
    after `--- end assembly ---`:
    `uart_putstr("AST peak: "); print_int(peak_nd);
    uart_putstr(" nodes, "); print_int(peak_chunks);
    uart_puts(" chunks");`.
* Temporarily bumped `CHUNK_MAX` in `src/chunk.h` from `16` to
  `64`. The bump is **only so the instrumentation can see the
  real peak** -- with the production `16` cap, `nd_alloc` short-
  circuits after 2,176 nodes and `peak_nd` would freeze, hiding
  the actual demand. Step 2 picks the production value from
  this audit's numbers; step 1 only measures.
* Built `build/plsw-meas.lgo` (1,310,414 bytes on disk; loads
  fine, runs fine).
* Drove it against:
  - All 15 reg-rs fixtures (carry-over from the 2026-05-10
    audit; same FILE:/SOURCE: protocol as `scripts/pipeline.sh`).
  - dcsno's `sno_util.plsw`, `sno_lex.plsw`, `sno_exec.plsw`
    from the sibling clone at
    `/disk1/.../work/dcsno/github/sw-embed/sw-cor24-snobol4`
    (group-readable, mode 660). Macros: `descr.msw`, `heap.msw`,
    `am.msw`, `pat.msw`, `snoglob.msw` -- the five every
    snobol4 module pulls via `%INCLUDE`.
  - `sno_engine.plsw` (the 137 KB consolidated module from the
    runtime-split saga) **was not reachable** from any clone
    visible to dcpls. Logged as a follow-up; the
    `dcpls-enlarge-src-buf.md` brief notes it lives on
    dcsno's `feat/runtime-split-resume`. Step 2 sizes against
    sno_exec (the biggest reachable proxy) plus an estimate
    for the consolidated case.

## Numbers

| Fixture / module | Peak `nd_count` | Peak `ast_chunk_count` | vs old 2,176-node ceiling |
|---|---:|---:|---:|
| hello                  |     8 |  1 |   0.4% |
| hello_macro            |    15 |  1 |   0.7% |
| macro                  |    22 |  1 |   1.0% |
| select_demo            |    34 |  1 |   1.6% |
| led                    |    57 |  1 |   2.6% |
| define                 |    70 |  1 |   3.2% |
| loop                   |    70 |  1 |   3.2% |
| chain                  |   154 |  2 |   7.1% |
| record                 |   159 |  2 |   7.3% |
| select_nested          |   175 |  2 |   8.0% |
| storage_basic          |   343 |  3 |  15.8% |
| storage_double_free    |   350 |  3 |  16.1% |
| storage_size_mismatch  |   350 |  3 |  16.1% |
| storage_oom            |   353 |  3 |  16.2% |
| storage_coalesce       |   406 |  3 |  18.7% |
| **sno_util.plsw**      | **1,913** | **15** |  **87.9%** |
| **sno_lex.plsw**       | **5,032** | **37** | **231.3%** (**> ceiling**) |
| **sno_exec.plsw**      | **6,008** | **45** | **276.1%** (**> ceiling**) |
| `sno_engine.plsw`      | not reachable | -- | -- |

The worst-case **measurable** input is `sno_exec.plsw` at **6,008
nodes / 45 chunks**. Both `sno_lex.plsw` and `sno_exec.plsw` blow
past the old 2,176-node ceiling -- which is exactly how the
regression manifested in production.

`sno_util.plsw` at 15 chunks (~88% of the old ceiling) is
*also* a near-miss -- the old measurement methodology would
have caught the regression if it had included even just
`sno_util` (a publicly-visible, in-snobol4-`src/` file). The
miss was specifically about not including any non-reg-rs input
in step 1 of the prior saga.

## Estimating `sno_engine.plsw`

Per `dcpls-enlarge-src-buf.md`, the consolidated
`sno_engine.plsw` is `sno_util` + `sno_lex` + `sno_exec` merged
(137,994 bytes single source). If AST node counts add roughly
linearly with combined source (they don't quite -- consolidation
removes some inter-module boilerplate -- but it's the right order
of magnitude), the consolidated peak is approximately:

```
sno_util + sno_lex + sno_exec
   1,913  +  5,032  +  6,008  = 12,953 nodes
                                ~95 chunks (at 136 nodes/chunk)
```

This estimate is the sizing input for `sno_engine.plsw` until
that file becomes reachable from this clone and a direct
measurement supplants it.

## Decision: production `CHUNK_MAX`

Two constraints:

1. **>= peak * 1.5** (mike's brief's headroom rule). Worst
   measurable is `sno_exec` at 6,008 nodes / 45 chunks; `* 1.5`
   = 9,012 nodes / **68 chunks**. Worst estimated is
   `sno_engine` at 12,953 / 95 chunks; `* 1.5` = ~143 chunks.
2. **>= 332 KB total pool** (so we don't structurally regress
   capacity vs. the pre-Phase-3 static AST allocation, which
   was `10 fields x 12,288 x 3 bytes = 368,640 bytes`, ~360 KB
   ASCII / 332 KB after rounding). At `CHUNK_SIZE = 4096`,
   `332 / 4 = 83 chunks` minimum.

The brief's primary suggestion is `CHUNK_MAX = 128` (= 512 KB
pool). That:

- **Covers `sno_exec` with 2.85x headroom** (128 / 45 = 2.84).
- **Covers the estimated `sno_engine` with 1.35x headroom**
  (128 / 95 = 1.35) -- below the 1.5x rule, but on a
  *projected* number that hasn't been measured.
- **Exceeds the 332 KB structural-regression floor** by 1.54x.
- **Matches the brief's recommendation** -- least surprise.
- **Total binary**: `872 KB - 64 KB (old pool) + 512 KB (new
  pool) = ~1,320 KB on disk`. Still smaller than the pre-Phase-3
  1,657 KB; well under the 1 MB SRAM physical ceiling for the
  *runtime* image because `chunk_storage`'s `.zero` regions
  encode sparsely in `.lgo` (the instrumented binary at
  CHUNK_MAX=64 was 1,310 KB on disk and loaded/ran fine).

If `sno_engine.plsw` ends up needing more than 128 chunks
(would require a direct measurement once that file is
reachable), `CHUNK_MAX = 160` (= 640 KB pool, 21,760-node cap,
~1.68x over the engine estimate) is the next-step bump. That
decision can be deferred until measurement says so.

**Step 2's planned value:** `CHUNK_MAX = 128`, with
`CHUNK_SIZE = 4096` unchanged. The `AST_NODES_PER_CHUNK = 136`
constant in `src/ast.h` stays put; only the chunk pool grows.

## Caveats

* Self-compile (PL/SW compiling itself) is still not
  measurable: the PL/SW compiler is written in C, not PL/SW.
  When a self-hosted compiler ever exists, re-measure.
* `peak_nd` is conservative: the bump allocator never frees
  intra-compile, so `peak_nd == final nd_count`, and
  `peak_chunks == ceil(peak_nd / 136)`. The numbers in the
  table above match this identity (e.g. `sno_exec`: 6008 / 136
  = 44.18 -> 45 chunks reported).
* If a future saga changes `AST_NODES_PER_CHUNK` (e.g. by
  bumping `CHUNK_SIZE`), the chunk-count column above needs
  re-derivation; the node-count column is invariant.
* The bumped CHUNK_MAX=64 in the instrumented binary is
  **measurement-only**. Step 1 reverts it to 16; step 2 sets
  it to 128 as the production value.

## Follow-ups

* **Add a regression test** (step 2): a synthetic large-AST
  fixture under `reg-rs/` that drives the AST pool to at least
  1.5x today's `sno_exec` peak. Without this, the same class
  of bug can recur silently.
* **Measure `sno_engine.plsw` directly** once it's reachable
  (dcsno's `feat/runtime-split-resume` lands, or a vendored
  copy gets dropped in this repo's tree). If the measured
  number exceeds the `CHUNK_MAX = 128` capacity, bump to 160
  in a follow-up step.
* **Consider exposing a `chunks_used=N/M` end-of-compile
  diagnostic** (step 2 optional task). Catches "too small"
  *and* "too large" regressions in CI.
