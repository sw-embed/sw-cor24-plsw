# Shrink `plsw.lgo` Size

The compiled PL/SW compiler at `build/plsw.lgo` is 1,738,122
bytes (1.7 MB) for what is, by design, "a tiny integer subset of
PL/I with no runtime, targeting an embedded 24-bit CPU." That is
unreasonable. Storage waste here is **gating dcsno's
`saga-expr-completeness` work and, downstream, dcftn**. This
document is the umbrella plan for reducing it.

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

### Phase 2 — AST chunk allocator (largest static win)

Replace the 10 × 12,288-slot parallel arrays with a chunk-based
node store: nodes allocated from a fixed chunk pool, freed at
end-of-compilation.

* Approach: Phase 2 of the brief, but with corrected sizing —
  start at `CHUNK_MAX = 4` × `CHUNK_SIZE = 65,536` chars =
  256 KB pool. Adjust based on real compilation node counts (a
  small `.plsw` program uses ~hundreds of nodes; biggest
  realistic input maybe a few thousand).
* Expected win: ~360 KB → ~256 KB pool, but the pool also subsumes
  `arena_buf` (~24 KB), allows shrinking `src_buf`/`inc_buf`/
  `mac_gen_buf` if they migrate to chunks too. Net ~150–200 KB
  off .data.
* Risk: AST access is in many call sites; the migration touches
  lots of code. Lint discipline (Phase 5) should land first or
  alongside to prevent regressions.

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

1. `test-split` — Phase 1.
2. `chunk-allocator` — Phase 2 scaffolding (allocator + tests,
   no AST migration yet).
3. `ast-to-chunks` — Phase 2 migration of AST.
4. `buffer-to-chunks` — Phase 3 (optional; only if dcsno still
   needs more headroom).
5. `static-lint` — Phase 5.
6. `shrink-lgo-verify` — Phase 6.

Ship in order; each unblocks the next. dcsno's
`saga-expr-completeness` step 003 unblocks earlier — likely
right after Phase 1 (test split), since splitting tests gives
the production compiler all the SRAM headroom it needs without
any allocator work.

## Out of scope (still)

* No tc24r ABI change. Chars are already packed.
* No COR24 ISA change.
* No `cor24-asm` or `link24` change.
* No code-size optimization in tc24r itself (separate question,
  separate brief if ever needed).
