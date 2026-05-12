# Shrink `plsw.lgo` Size

The compiled PL/SW compiler at `build/plsw.lgo` is 1,738,122
bytes (1.7 MB) for what is, by design, "a tiny integer subset of
PL/I with no runtime, targeting an embedded 24-bit CPU." That is
unreasonable. Storage waste here is **gating dcsno's
`saga-expr-completeness` work and, downstream, dcftn**. This
document is the umbrella plan for reducing it.

## Status (2026-05-12)

`build/plsw.lgo` is now **872,174 bytes** (sha256
`38774800…0affef`), a **~865 KB drop / ~50% reduction** from the
1,738,122-byte 2026-05-09 baseline. Phases 1, 2, and 2b are all
landed; the headline yardstick of ≤ 700 KB is still 172 KB away,
but the new total leaves COR24's 1 MB SRAM with ~150 KB of
headroom plus the full 64 KB chunk pool's worth of dynamic
working space — enough that **dcsno's `saga-expr-completeness`
and dcftn are no longer gated by plsw.lgo size**. Phase 3
(`buffer-to-chunks`) is now optional, decided after dcsno/dcftn
report whether they need the additional headroom.

This doc supersedes the constraint #1 framing in
`tools/briefs/dcpls-dynamic-memory-architecture.md`. The brief's
phase structure (0..6) is kept, but its sizing arithmetic is
corrected here.

## Corrected analysis

### What I had wrong before

An earlier reading of the architecture brief said:

> "char is 1 word (24 bits) on COR24. Every char in a buffer
> costs 3 bytes. emit_buf[262144] was 786 KB physical."

That is **false**. tc24r already packs `char` arrays 3-per-word
in COR24 memory, and the ABI is `sizeof(char) == 1`. The brief's
"3× tax" doesn't exist.

### What is actually true

Confirmed by reading `build/plsw.s` directly:

* `emit_buf[4096]` (post-streaming-emit) emits **1366 `.word`**
  directives (4096 / 3 = 1365.33). Physical size: ~4,098 bytes.
* `src_buf[65536]` emits **21,846 `.word`** directives
  (65536 / 3 = 21845.33). Physical size: ~65,538 bytes.
* `nd_kind[12288]` (an `int` array, not `char`) emits **12,288
  `.word`** directives — exactly one word per element. Confirms
  ints are 1 word; chars are packed.

Cross-check against tc24r's own ABI gates in
`sw-cor24-x-tinyc/demos/`: demo18, demo29, demo34, demo35 all
assert `sizeof(char) == 1` and char-array sums work in bytes
(not words). The ABI is settled.

### Implication

* No tc24r change is needed for this work.
* The 4 KB-per-static-block discipline from the brief is still
  the right rule, but the per-block sizing reasoning shifts: the
  ceiling is about *fairness and SRAM headroom*, not about a
  fictional 3× tax.
* Phase 2's proposed `chunk_storage[CHUNK_MAX * CHUNK_SIZE]` =
  `[16 * 65536]` = 1,048,576 chars = **~1 MB physical** (not the
  ~3 MB I mistakenly cited). It still wants tuning down — 1 MB
  consumes the entire SRAM budget — but it is feasible and not
  catastrophic.

## Baseline measurement (Phase 0)

Build under measurement: `build/plsw.lgo`
SHA-256 `b6525ebe7c2e3c0dfd073dc5fe765bf8786f3672c0adf2e85bbf4287ac081d11`
(1,738,122 bytes, post-streaming-emit, 2026-05-09).

`build/plsw.s` totals 269,972 lines, of which line 1 .. 56293 is
`.text` (~56 K lines) and 56294 .. 269972 is `.data` (~214 K
lines). At one COR24 word per `.word` directive (3 bytes), .data
physical is approximately **640 KB**, .text physical (after
linking instruction bytes, 1/2/4 each) is approximately
**1.1 MB**.

### Top static-storage contributors

Largest `.data` symbols by `.word` line count, grouped by
purpose. Physical bytes ≈ words × 3.

#### AST node pool (`src/ast.h`, `NODE_POOL_MAX = 12288`)

| Symbol | Words | Bytes |
|---|---:|---:|
| `nd_kind`  | 12,288 | 36,864 |
| `nd_type`  | 12,288 | 36,864 |
| `nd_stor`  | 12,288 | 36,864 |
| `nd_level` | 12,288 | 36,864 |
| `nd_left`  | 12,288 | 36,864 |
| `nd_right` | 12,288 | 36,864 |
| `nd_next`  | 12,288 | 36,864 |
| `nd_ival`  | 12,288 | 36,864 |
| `nd_line`  | 12,288 | 36,864 |
| `nd_name`  | 12,288 | 36,864 |
| **subtotal** | **122,880** | **~360 KB** |

10 parallel int arrays, one slot per AST node. Statically sized
for the worst case; most compilations use a small fraction.

#### Symbol-table pool (`src/symtab.h`, `SYM_TOTAL = 2048`)

8 parallel arrays × 2,048 entries = 16,384 words ≈ **48 KB**.

#### Type-descriptor pool (`src/types.h`, `TDESC_MAX = 64`, plus field arrays)

`td_*` and `td_f*` arrays total ~4,100 words ≈ **12 KB**.

#### Char buffers

| Symbol | Decl size | Words | Bytes |
|---|---:|---:|---:|
| `src_buf`         | 65,536 | 21,846 | ~65,538 |
| `mac_gen_buf`     | 49,152 | 16,384 | ~49,152 |
| `inc_buf`         | 32,768 | 10,923 | ~32,769 |
| `arena_buf`       | 24,576 |  8,192 | ~24,576 |
| `emit_buf`        |  4,096 |  1,366 |  ~4,098 |
| `mac_cl_name_buf` |  2,048 |    683 |  ~2,049 |
| `mac_body_buf`    |  2,048 |    683 |  ~2,049 |
| `mac_arg_buf`     |    512 |    171 |    ~513 |
| `mac_expand_buf`  |    512 |    171 |    ~513 |
| `mac_name_buf`    |    256 |     86 |    ~256 |
| `cur_text`        |    256 |     86 |    ~256 |
| **subtotal**      |        |  60,591 | **~182 KB** |

#### Pointer & misc small arrays

`def_names`/`def_values` (512 ptrs each = 1,024), `src_line_ptr`
(256 ptrs), `cg_str_label`/`cg_str_data` (128 each), keywords
(37 each), and a long tail of small statics. Subtotal ~**5 KB**.

### Static-storage totals

| Category | Bytes | % of .data |
|---|---:|---:|
| AST node pool             | ~360 K | ~56% |
| Char buffers (above)      | ~182 K | ~28% |
| Symtab pool               |  ~48 K |  ~7% |
| Type-descriptor pool      |  ~12 K |  ~2% |
| Pointer / small arrays    |   ~5 K |  ~1% |
| Other (long tail)         |  ~33 K |  ~5% |
| **.data total**           | **~640 K** |   |

### Code (.text)

`src/main.c` is 7,075 lines, of which 46 are `void test_*()`
functions (the bulk: ~6,800 lines of test bodies, `uart_putstr`
banners, and string literals). The rest of `src/*.h` is roughly
6,200 lines of compiler proper. Compiled `.text` is ~1.1 MB
total, dominated by test code.

### Whole-file headline

| Section | Physical |
|---|---:|
| `.text` (code)             | ~1.1 MB |
| `.data` (statics)          | ~0.6 MB |
| **`build/plsw.lgo` total** | **1,738,122 bytes** |

## Reduction roadmap

The brief's phases are still right; reordered to start with the
biggest, surest wins given the corrected sizing.

### Phase 0 — Measure (this saga, this step)

* Capture baseline above. Done.
* Define a yardstick: **post-shrink target ≤ 700 KB `plsw.lgo`**
  with no functional regression. ~60% reduction.

### Phase 1 — Test split (largest single win, low risk) — DONE 2026-05-10

Shipped via `pr/test-split`. 46 `test_*` functions and
`run_suite()` moved from `src/main.c` into a new
`src/test_main.c`. The shared `compile_program()` driver and
its COR24 runtime-preamble emitters moved into `src/compile.h`
so both binaries can call them without duplicating the body.

* Production `plsw.lgo`: 1,738,122 → **1,511,582 bytes**
  (~226 KB / 13% saved). Less than the speculated ~900 KB:
  the test code was ~6,800 lines but compiled to only ~226 KB
  of `.text` because most lines were calls to `uart_puts` /
  `print_int` / fixture string literals, not dense computation.
* `.text` line count in `build/plsw.s` halved (56,293 → 27,386
  lines), confirming the test-text removal even though .lgo
  byte savings were smaller than line-count would suggest.
* Production binary still has full `.data` (AST pool, symtab,
  buffers) — those are header-declared globals shared by both
  binaries. Phase 2 is what shrinks `.data`.
* `just test` (reg-rs) unchanged: 15/15 green. It drives the
  production compiler against `.plsw` fixtures via
  `pipeline.sh` and never touched the in-binary suites.
* New build target: `just build-test-lgo` produces
  `build/plsw_test.lgo` for hands-on debugging of in-binary
  suites.

### Phase 2 — Chunk allocator scaffolding — DONE 2026-05-10

Shipped as the `chunk-allocator` saga (three steps):

* `pr/chunk-api` — `src/chunk.h` with the API + 64 KB pre-reserved
  pool (`CHUNK_SIZE = 4096`, `CHUNK_MAX = 16`). Header-only, matches
  `arena.h`'s convention. Included from `main.c` and `test_main.c`.
* `pr/chunk-tests` — new suite #37 `test_chunk` in `src/test_main.c`
  covering init, alloc-distinct, exhaustion, free+reuse,
  free-of-unknown. Verified `chunk errors: 0` under `cor24-emu`.
* `pr/chunk-baseline` — this entry.

**Sizing (smaller than both the brief and the original Phase 2
plan).** Per discussion 2026-05-10, the pool is `16 × 4 KB =
64 KB`, not `4 × 64 KB = 256 KB` as originally written. Each
chunk equals the 4 KB static-block ceiling — the granularity
that Phase 5's lint script will enforce. 64 KB is the entire
dynamic-memory budget for the production compiler; bump
`CHUNK_MAX` only if `ast-to-chunks` measurements demand it.

**API.** `chunk_init` / `chunk_alloc` / `chunk_free` / `chunk_used`.
`chunk_alloc` returns `0` (NULL) on exhaustion; `chunk_free` is a
silent no-op on NULL or any unknown pointer (no trap path on
freestanding COR24).

**Static cost recorded.** `build/plsw.lgo` grew from 1,511,582
(post-`test-split`) to **1,657,430 bytes** (+145,848 bytes /
~14%). `build/plsw_test.lgo` is 1,663,430 bytes. The growth
breakdown:

* `chunk_storage` lands as `.zero 65536` in `build/plsw.s`
  (single directive — tc24r elides the explicit `.word` run
  when there's no static initializer). The source-level
  declaration carries `/* lint-exempt: chunk-pool */`. The
  Phase 5 compiled-output lint will need to recognise this
  symbol's exemption when it scans for `.zero N > 4096`.
* `chunk_table` lands as `.zero 96` (16 entries × 6 bytes).
* tc24r's function-level DCE elides `chunk_init`/`alloc`/`free`/
  `used` from `build/plsw.s` (no callers yet) — they appear only
  in `build/plsw_test.s` where `test_chunk` uses them.

**No headline shrink yet — by design.** Phase 2 is pre-reservation
infrastructure. The actual reclaim lands in `ast-to-chunks` when
the 360 KB AST static pool migrates onto these chunks.

### Phase 2b — Migrate the AST onto chunks — DONE 2026-05-12

Shipped as the `ast-to-chunks` saga (four steps):

* `pr/measure-ast` — Phase 0 measurement step.
  `#ifdef MEASURE`-instrumented `nd_alloc()` ran the production
  compiler against all 15 reg-rs fixtures; peak AST node count
  was **406** (storage_coalesce fixture), min 8 (hello).
  Captured in `docs/memory-audit-2026-05-10.md`. Decision:
  keep `CHUNK_MAX = 16`; the 2,176-node ceiling has **5.4×
  headroom** over the worst-case fixture.
* `pr/ast-accessors` — introduced the ten `nd_kind(i)` /
  `nd_type(i)` / … function-like accessor macros and migrated
  all 406 `nd_*[i]` callsites across `src/ast.h`,
  `src/types.h`, `src/parser.h`, `src/layout.h`,
  `src/codegen.h`, `src/test_main.c`. Storage was unchanged in
  this step — the macros initially resolved to the same parallel
  arrays — so the layer was proven before storage moved.
* `pr/ast-chunk-storage` — switched the macros to chunk-backed
  storage. The 10 static `nd_*_arr[NODE_POOL_MAX]` arrays were
  replaced with a `struct ast_block` of parallel sub-arrays
  drawn from `chunk_alloc()`, indexed via `(i /
  AST_NODES_PER_CHUNK, i % AST_NODES_PER_CHUNK)`. `ast_init()`
  returns chunks to the pool between compiles. `chunk_init()`
  is now called at the top of `compile_program()` and once at
  test-runner startup.
* `pr/ast-baseline` — this entry.

**Layout chosen.** `AST_NODES_PER_CHUNK = 136`,
`AST_CHUNKS_MAX = CHUNK_MAX = 16`. Per-node footprint is 10
fields × 3 bytes = 30 bytes; 136 × 30 = 4080 bytes per chunk,
fitting one 4 KB chunk with 16 bytes of slack.
Effective node cap: **2,176** (vs. the old static cap of
12,288). Peak observed chunk usage: **3 of 16** (406 nodes ÷
136 ≈ 2.98, rounds up to 3) — plenty of room for larger
programs and concurrent AST/other-pool consumers when Phase 3
arrives.

**Static cost reclaimed.** `build/plsw.lgo` dropped from
1,657,430 bytes (post-`chunk-allocator`) to **872,174 bytes**
(saved **785,256 bytes / ~47%**). The drop overshoots the
original ~296 KB projection by a wide margin: the assembler's
encoding of the ten `int nd_*_arr[12288]` declarations carried
significant overhead beyond the raw 360 KB the size doc had
estimated for `.data`, and removing them collapsed the `.s`
text as well. `build/plsw_test.lgo` dropped from 1,663,376 to
**884,680 bytes** (-778,696 bytes / ~47%).

**SHAs.**

* `build/plsw.lgo` sha256
  `3877480056c5ded09a0891305ae2574b214aa3cef4142c65fc049917720affef`
  (872,174 bytes).
* `build/plsw_test.lgo` sha256
  `7ffd684c01c006cff1e79ba2e04f4c9e2cc4e06400a8f8f77b2c5f6a18375a3d`
  (884,680 bytes).

**Behavior change at the cap boundary.** On AST exhaustion, the
old "AST node pool exhausted (12288 nodes)" message is replaced
by "AST pool exhausted (chunk table full)" or, when the chunk
pool itself is empty, "chunk_alloc returned NULL for AST node".
Downstream consumers should treat this as the rebuild-required
signal alongside the .lgo SHA change.

**No semantic regression.** `just test` (reg-rs) stays 15/15
green. Emulator suites 4 (AST), 5 (parser), 14 (codegen,
`cg_err=0`), 17 (proc codegen), 36 (SELECT/WHEN), 37 (chunk
allocator, 0 errors) all pass. The all-suite (`a`) run
confirms reset hygiene across 30+ `ast_init()` cycles — chunks
return cleanly to the pool between back-to-back compiles, so a
16-chunk pool sustains arbitrary REPL workloads.

**Out of scope, but observed.** Suites 31–35 (hello-world /
LED-toggle / counted-loop / record-pointer / multi-based
compile-roundtrip) have a pre-existing failure pattern: the
streaming-emit contract in `compile_program()` flushes most of
the `.s` to UART before the test sees `emit_output()`, so
`str_find` on the returned tail finds nothing. Verified against
the pre-saga baseline — same 8 failures there. Filed as a
candidate for a future saga (rework those tests to capture
streamed output, or buffer the full emission for tests).

### Phase 3 — Migrate buffers to chunks (bonus on Phase 2)

Once chunks exist, `src_buf`, `inc_buf`, and `mac_gen_buf` can
also become chunk-borrowed instead of static. Not required for
the headline target but cleans up the static layout.

### Phase 4 — Streaming buffers (already done)

`pr/streaming-emit` already replaced the 256 KB `emit_buf` with
a 4 KB coalescer. Saved ~252 KB. Closed.

### Phase 5 — Audit + lint

Add a build-time lint rule: any static array > 4 KB physical
must carry a `/* lint-exempt: <reason> */` comment. Chunk pool
gets `lint-exempt: chunk-pool`. Streaming buffers get
`lint-exempt: streaming-coalescer`. Anything else is a build
error. Prevents regressions.

### Phase 6 — Verify

Final sweep: rebuild, capture new SHA, confirm headline target
hit, regenerate goldens that depend on `.s` text, document the
new baseline in `CHANGES.md`.

## Saga sequencing

Each phase is a separate saga. This saga (`shrink-lgo`) has only
the plan-and-baseline step; subsequent sagas:

1. `test-split` — Phase 1. **DONE 2026-05-10.**
2. `chunk-allocator` — Phase 2 scaffolding (allocator + tests,
   no AST migration yet). **DONE 2026-05-10.**
3. `ast-to-chunks` — Phase 2b migration of AST.
   **DONE 2026-05-12.**
4. `buffer-to-chunks` — Phase 3 (optional; only if dcsno/dcftn
   report needing more headroom after rebuilding on the
   post-2b plsw.lgo).
5. `static-lint` — Phase 5.
6. `shrink-lgo-verify` — Phase 6.

Ship in order; each unblocks the next. dcsno's
`saga-expr-completeness` step 003 unblocked after Phase 1 (test
split). Phase 2b's ~785 KB reclaim now also clears the dcftn
gate — `plsw.lgo` at 872 KB leaves ample SRAM for runtime work.

## Out of scope (still)

* No tc24r ABI change. Chars are already packed.
* No COR24 ISA change.
* No `cor24-asm` or `link24` change.
* No code-size optimization in tc24r itself (separate question,
  separate brief if ever needed).
