# PL/SW Storage Allocation Design

**Status:** design only — no `plsw_storage.{msw,plsw}` exists yet.
This doc captures the layering and contract; implementation lands
in a future saga.

## Background

PL/SW today is freestanding: no `malloc`, no `free`, no runtime.
Programs that need heap storage roll their own bump arena, per
the canonical pattern in `examples/chain.plsw:14-23`:

```pl/i
DCL ARENA(512) BYTE;
DCL ARENA_POS INT INIT(0);

ALLOC: PROC(SIZE INT) RETURNS(INT);
    DCL BASE INT;
    BASE = ADDR(ARENA) + ARENA_POS;
    ARENA_POS = ARENA_POS + SIZE;
    RETURN(BASE);
END;
```

That works for `chain.plsw`'s four allocations. It does not work
for the SNOBOL4 interpreter, which (when running `compiler.sno`
or anything pattern-heavy) does many small short-lived
allocations and needs both per-cell free and bulk reclaim. Asking
each PL/SW consumer to reinvent this is the wrong answer.

The intuition for `?GETMAIN`/`?FREEMAIN` macros (modeled on
PL/X / Real Storage Manager) is right. The design question is
how much PL/SW should provide and how much each consumer should
own.

## Layering rule

**PL/SW provides only the raw alloc/free primitives. Region
boundaries, mark/reclaim, and garbage collection live in the
consumer's runtime.**

Concretely:

| Concern | Layer |
|---|---|
| `?GETMAIN(LENGTH(n), ADDRESS(p), RC(rc))` macro + impl | **PL/SW** (`plsw_storage.{msw,plsw}`) |
| `?FREEMAIN(ADDRESS(p), LENGTH(n), RC(rc))` macro + impl | **PL/SW** |
| Free-list / size-class / fragmentation policy | **PL/SW** (implementation detail of `_PLSW_GETMAIN`) |
| Region-save / region-restore / watermark reclaim | **consumer** |
| Mark phase, sweep phase, GC root traversal | **consumer** |
| Object headers, type tags, refcounts, generations | **consumer** |
| Per-arena / per-thread / per-fiber heaps | **consumer** |

Rationale: SNOBOL4 wants GC. Prolog wants WAM-style trail
unwinding (which is closer to a watermark reclaim than to
malloc/free). Future Fortran wants common-block-style static
plus stack frames and probably nothing else. Future C-like
guests want explicit `malloc`/`free`. **Each of these has a
different storage policy. They share only the underlying need
for "give me N bytes" and "I'm done with these N bytes."** PL/SW
is the substrate; policy is built on top.

This is also why there is no `?RECLAIM`/`?MARK`/`?REGION_SAVE`
macro in PL/SW. Region-style reclaim is a SNOBOL4 (or Prolog,
or whoever) concern, implemented as ordinary procedures in the
consumer's runtime, not as a PL/SW macro that everyone has to
agree on.

## Macro contract

Two macros, both keyword-argument, modeled on the PL/X spelling:

```pl/i
?GETMAIN(LENGTH(expr), ADDRESS(lvalue), RC(lvalue));
?FREEMAIN(ADDRESS(expr), LENGTH(expr), RC(lvalue));
```

| Clause | Required? | Type | Semantics |
|---|---|---|---|
| `LENGTH(expr)` | required for both | INT (bytes) | size of the block to allocate / free |
| `ADDRESS(...)` | required for both | PTR | `?GETMAIN` writes the allocated address to its lvalue; `?FREEMAIN` reads the address to free |
| `RC(lvalue)` | optional | INT | return code: 0 = success, non-zero = failure (out of memory on `?GETMAIN`; double-free / invalid pointer on `?FREEMAIN`). If omitted, the macro silently ignores failures. |
| `SUBPOOL(expr)` | reserved | INT | not in v1; reserved for a future multi-arena extension |

Expansion target: a normal procedure call sequence. The COR24
ISA has no SVC-equivalent instruction, so `?GETMAIN` expands to
something like:

```
push    LENGTH                ; arg
jal     r1, _PLSW_GETMAIN     ; r0 = address (or 0 on OOM)
sw      r0, ADDRESS           ; store result
ceq     r0, z                 ; rc = (r0 == 0) ? 1 : 0
…
```

(Exact expansion is an implementation concern; the contract is
"after `?GETMAIN(LENGTH(n), ADDRESS(p), RC(rc))`, `p` holds an
n-byte block on success or undefined on failure, and `rc` is 0
on success or non-zero on failure.")

Identical pattern for `?FREEMAIN`. Both run in O(1) caller-side
(one push, one call, one store); the allocator's complexity is
inside `_PLSW_GETMAIN`/`_PLSW_FREEMAIN`.

## Two-file opt-in pattern

PL/SW separate-compilation already supports the pattern via
`%DEFINE LIBRARY` modules linked through `meta-gen` + `link24`.
Storage uses the same shape:

```
plsw_storage.msw          declares ?GETMAIN/?FREEMAIN macros + EXTERNAL
                          decls of _PLSW_GETMAIN, _PLSW_FREEMAIN

plsw_storage.plsw         %DEFINE LIBRARY: provides _PLSW_GETMAIN,
                          _PLSW_FREEMAIN, and the heap region itself.
                          Heap size is configurable via %DEFINE
                          PLSW_HEAP_SIZE before %INCLUDE.
```

A consumer opts in by:

1. `%INCLUDE plsw_storage;` in their `.msw` (or directly in their
   `.plsw` if they don't have a header layer).
2. Adding `plsw_storage` to their link line (the modules list
   passed to `meta-gen` + `link24`).
3. Optionally declaring the heap size before include:
   ```pl/i
   %DEFINE PLSW_HEAP_SIZE 16384;
   %INCLUDE plsw_storage;
   ```

A program that doesn't `%INCLUDE plsw_storage` and doesn't link
in `plsw_storage.plsw` pays nothing — no heap region, no
allocator code, no impact on `.lgo` size. Opt-in is the contract,
not a build-time flag.

## Backend: free-list with embedded headers

PL/SW's `_PLSW_GETMAIN` is a **free-list allocator** with
embedded per-block metadata. This is the lowest-common-denominator
backend that supports both alloc and free with constant-time-ish
behavior:

- The heap region (`PLSW_HEAP_SIZE` bytes) is divided into
  variable-length blocks. Each block has a small header
  (~6 bytes: a `size` and a `next` pointer when free).
- Initially the heap is one giant free block.
- `_PLSW_GETMAIN(n)` walks the free list (first-fit), splits the
  chosen block into "allocated `n+header` bytes" and "remainder
  back on the free list," and returns the address past the
  header. Returns 0 if no block fits.
- `_PLSW_FREEMAIN(addr, n)` reads the block's header to find
  size, links it back into the free list, and coalesces with
  the next neighbor if it's also free.

Why free-list rather than bump or slab:

| Backend | Per-cell `?FREEMAIN` works? | Code size | Per-block overhead | Verdict |
|---|---|---|---|---|
| Bump-only | No (FREEMAIN is a no-op) | ~30 lines | 0 bytes | Doesn't satisfy the `?FREEMAIN` contract; consumers needing real free have to roll their own anyway |
| Free-list | Yes | ~150 lines | ~6 bytes/block | **Chosen.** Honors both macros' contracts at modest cost. |
| Size-class slab | Yes (within a class) | ~300+ lines | 0–4 bytes/block | Better for tiny-allocation churn but most code; defer until profiled need |

The free-list backend is intentionally simple: first-fit, single
free list, coalesce on free, no splitting heuristics, no buddy
system. For consumers that find this too slow or too fragmented
(SNOBOL4 may), the answer is to layer their own pool / region /
GC over the top, *not* to grow the PL/SW backend. PL/SW's
allocator is the dumb substrate.

If profiling later shows that `?GETMAIN`/`?FREEMAIN` per cell is
the actual bottleneck (rather than e.g. the lack of a region
allocator on top), a future saga can introduce a size-class slab
fast path. That's a transparent backend change — the macro
contract doesn't move.

## What consumers do on top

Consumers layer their storage policy as ordinary PL/SW code,
calling `?GETMAIN` / `?FREEMAIN` (or the underlying procedures)
as primitives.

**SNOBOL4** (the immediate user) likely wants:
- A region allocator built atop `?GETMAIN` for pattern-match
  scratch (save the heap watermark, do match work that allocates
  cells, on backtrack `?FREEMAIN` everything since the watermark).
- OR: a single big `?GETMAIN(LENGTH(BIG))` for the SNOBOL4 heap,
  with SNOBOL4's runtime managing the entire region with its own
  cell allocator and mark/sweep GC, never calling `?FREEMAIN`
  for individual cells.
- Either is valid; both treat PL/SW's `?GETMAIN`/`?FREEMAIN` as
  a substrate. See the dcsno brief at
  `tools/briefs/dcsno-storage-allocation-runtime.md` for the
  detailed shape.

**Prolog** (future) likely wants WAM-style trail-unwind reclaim,
which maps onto a stack-of-arenas pattern over `?GETMAIN`.

**C-like guests** (future) would call `?GETMAIN`/`?FREEMAIN`
directly from their `malloc`/`free` shims.

## Out of scope (explicit)

The following are *not* PL/SW concerns and will not be added to
`plsw_storage.{msw,plsw}`:

- `?RECLAIM`, `?REGION_SAVE`, `?REGION_RESTORE`, watermark/mark
  primitives. **Build these in your runtime.**
- Garbage collection: mark, sweep, copy, generational, refcount.
  **Build this in your runtime.**
- Per-arena or per-context heaps. **Build this in your runtime
  using multiple `?GETMAIN(LENGTH(BIG))` regions if needed.**
- Object headers, type tags, refcounts. **Your runtime, your
  call.**
- `realloc`-equivalent. **Use `?GETMAIN` + copy + `?FREEMAIN` if
  needed; PL/SW won't ship a primitive for it in v1.**
- Multi-threaded heap safety. **COR24 is single-threaded; no
  locking is required.**

## Implementation roadmap

This doc is design-only. Future sagas:

1. **dcpls/`pr/storage-macros`** — implement `plsw_storage.msw` +
   `plsw_storage.plsw` with the free-list backend. Add
   regression tests under `reg-rs/` exercising alloc, free,
   coalesce, OOM, double-free.
2. **dcsno/`pr/storage-allocation-runtime`** — SNOBOL4 builds its
   region/mark/reclaim runtime over PL/SW's primitives.
   See `tools/briefs/dcsno-storage-allocation-runtime.md`.
3. (Future) Other consumers (Prolog, etc.) layer their policy
   the same way.

## See also

- `docs/storage-research.txt` — the design conversation that led
  to this doc.
- `docs/architecture.md` (Memory Layout section) — where the
  heap sits in the COR24 address space.
- `examples/chain.plsw` — current ad-hoc bump-arena pattern,
  which `?GETMAIN`/`?FREEMAIN` replaces for non-trivial
  consumers.
- `tools/briefs/dcsno-storage-allocation-runtime.md` — the
  forwarded brief for SNOBOL4's runtime saga.
