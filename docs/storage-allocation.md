# PL/SW Storage Allocation Design

**Status:** v1 + macros shipped via `include/_plsw_storage.msw`.
Procedures `_PLSW_GETMAIN` / `_PLSW_FREEMAIN` are functional, and
the PL/X-style `?GETMAIN` / `?FREEMAIN` macros wrap them as
source-template MACRODEFs (`{KEY}` placeholders, body re-fed to
the lexer/parser at expansion). Reg-rs cases
`plsw_storage_basic`, `plsw_storage_coalesce`, `plsw_storage_oom`,
`plsw_storage_double_free`, `plsw_storage_size_mismatch` exercise
the macros.

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

## Procedure contract (v1)

Two procedures, called as ordinary PL/SW functions:

```pl/i
P  = _PLSW_GETMAIN(SIZE);
RC = _PLSW_FREEMAIN(P, LEN);
```

| Procedure | Args | Return |
|---|---|---|
| `_PLSW_GETMAIN(SIZE INT) RETURNS(INT)` | `SIZE`: bytes of user data wanted | address of user block (after the 6-byte header), or `0` on out-of-memory |
| `_PLSW_FREEMAIN(USERADDR INT, LEN INT) RETURNS(INT)` | `USERADDR`: address from `_PLSW_GETMAIN`. `LEN`: same size that was requested. | `0` on success; `1` on double-free / invalid pointer / corrupted header; `2` on size mismatch (`LEN` doesn't match the recorded size) |

`LEN` is required on free, mirroring IBM OS/MVS / OS/390
FREEMAIN. The implementation cross-checks `LEN` against the size
stored in the in-band header — mismatch returns `RC=2`. (See
"IBM heritage" note below for context.)

`SIZE=0` allocates a 6-byte block (header only, zero user data);
the returned address is valid and `_PLSW_FREEMAIN(addr, 0)`
frees it. No special case.

## Macro contract (shipped in v1)

```pl/i
?GETMAIN(LENGTH(<expr>), ADDRESS(<lvalue>), RC(<lvalue>));
?FREEMAIN(LENGTH(<expr>), ADDRESS(<lvalue>), RC(<lvalue>));
```

| Clause | Type | Semantics |
|---|---|---|
| `LENGTH(expr)` | INT (bytes) | size of the block to allocate / free |
| `ADDRESS(lvalue)` | PTR | `?GETMAIN` writes the allocated address to its lvalue; `?FREEMAIN` reads the address to free |
| `RC(lvalue)` | INT | return code, written by both macros: `?GETMAIN` 0=ok / 4=OOM (PL/X-style); `?FREEMAIN` passes through `_PLSW_FREEMAIN`'s return (0/1/2) |

`RC` is **required** in v1. PL/SW's macro system doesn't yet
support `%IF DEFINED(RC)` inside source-template bodies, which
is what an OPTIONAL clause would need to gate its emission.
Future enhancement: relax to OPTIONAL once that lands.

### How the expansion works

PL/SW's macro system supports two body forms inside `MACRODEF`:

- `GEN DO; '<asm-line>'; ...; END;` — assembly emission, the
  existing pattern used by `?LED_SET`, `?GREET`, `?NOP`.
- A bare body of PL/SW source statements (no `GEN` wrapper) —
  source emission. `{KEY}` placeholders are substituted with
  the caller's argument values; the result is re-fed to the
  lexer/parser at the call site, so the rest of the compiler
  handles type-checking and codegen normally.

`?GETMAIN` and `?FREEMAIN` use the source-emission form.
Concretely (from `include/_plsw_storage.msw`):

```pl/i
MACRODEF GETMAIN;
    REQUIRED LENGTH(expr);
    REQUIRED ADDRESS(lvalue);
    REQUIRED RC(lvalue);
    {ADDRESS} = _PLSW_GETMAIN({LENGTH});
    IF ({ADDRESS} = 0) THEN {RC} = 4;
    ELSE {RC} = 0;
END;

MACRODEF FREEMAIN;
    REQUIRED LENGTH(expr);
    REQUIRED ADDRESS(lvalue);
    REQUIRED RC(lvalue);
    {RC} = _PLSW_FREEMAIN({ADDRESS}, {LENGTH});
END;
```

`?GETMAIN(LENGTH(12), ADDRESS(P), RC(rc))` expands (textually)
to:

```pl/i
P = _PLSW_GETMAIN(12);
IF (P = 0) THEN rc = 4;
ELSE rc = 0;
```

…which the parser handles exactly as if the user had written it
by hand.

### Procedure form (still available)

For callers that want raw control over RC handling or that
benefit from explicit return-value inspection:

```pl/i
P  = _PLSW_GETMAIN(SIZE);            /* P = address or 0 on OOM */
RC = _PLSW_FREEMAIN(P, LEN);         /* RC = 0/1/2 */
```

Same semantics as the macros; the macros are convenience wrappers.

## Single-file opt-in pattern

PL/SW v1 ships storage as a single inline `.msw`:

```
include/_plsw_storage.msw     macros (deferred), procedures, and
                              heap region. %INCLUDE'ing this file
                              text-concats everything into the
                              entry module.
```

The leading underscore in the filename (`_plsw_`) reserves
`storage.{msw,plsw}` for application use — SNOBOL4 can ship a
`runtime/storage.plsw` for its own mark/reclaim layer without
clashing.

A consumer opts in by:

1. (optional) Choose a heap size:
   ```pl/i
   %DEFINE PLSW_HEAP_SIZE 16384;     /* default 65536 */
   ```
2. Include the runtime in the entry module:
   ```pl/i
   %INCLUDE _plsw_storage;
   ```
3. (multi-module builds only) For non-entry modules that need to
   *call* `_PLSW_GETMAIN` / `_PLSW_FREEMAIN` but should not emit
   their own copy of the impl + heap, set the headers-only guard
   before include:
   ```pl/i
   %DEFINE PLSW_STORAGE_HEADERS_ONLY 1;
   %INCLUDE _plsw_storage;
   ```
4. Pass `include/_plsw_storage.msw` on the pipeline command line
   (the `FILE:` protocol registers it for `%INCLUDE` resolution):
   ```sh
   ./scripts/pipeline.sh include/_plsw_storage.msw your_program.plsw
   ```

A program that doesn't `%INCLUDE _plsw_storage` pays nothing —
no heap region, no allocator code, no impact on `.lgo` size.
Opt-in is the contract, not a build-time flag.

### Why single-file inline (and not modular .msw + .plsw)

The original design considered a two-file pattern
(`storage.msw` + `storage.plsw` linked via `meta-gen` + `link24`).
That remains a viable layout for downstream consumers (SNOBOL4
already builds modular), but for the PL/SW runtime itself the
single-file inline form is simpler:

- Tiny demos (the reg-rs golden cases) stay on the simple
  single-file pipeline. No modular build for two-line tests.
- SNOBOL4 already pays the modular-build tax; adding storage as
  a `%INCLUDE _plsw_storage;` in `sno_main.plsw` (only) is one
  line. Other SNOBOL4 modules use the `PLSW_STORAGE_HEADERS_ONLY`
  guard.
- A future "promote storage to a separate library module" saga
  is a non-breaking change — the `?GETMAIN` / `?FREEMAIN`
  contract stays the same, only the build steps change.

## Backend: free-list with embedded headers

PL/SW's `_PLSW_GETMAIN` is a **free-list allocator** with
embedded per-block metadata. This is the lowest-common-denominator
backend that supports both alloc and free with constant-time-ish
behavior:

- The heap region (`PLSW_HEAP_SIZE` bytes, default 65536) lives
  in `_PLSW_HEAP_BUF[]`, a `BYTE` array declared at module scope.
- Each block has a 6-byte header at its start: 3-byte
  `BLOCK_SIZE` (total block size including header) followed by
  3-byte `BLOCK_NEXT` (when free: address of the next free block,
  or `0`; when allocated: `0xFFFFFF`, the alloc-magic sentinel).
- Free list is singly-linked, sorted by address ascending,
  pointed to by `_PLSW_FREE_HEAD`.
- Lazy init: first call to `_PLSW_GETMAIN` sets up one giant
  free block covering the entire heap. The `_PLSW_INIT_DONE`
  flag avoids re-init on subsequent calls.
- `_PLSW_GETMAIN(n)` walks the free list (first-fit), splits the
  chosen block into "allocated `n+6` bytes" and "remainder back
  on the free list" (only when the remainder leaves room for a
  useful block: `>= header + 1 byte`). Stamps the chosen block's
  `BLOCK_NEXT` with the alloc-magic sentinel. Returns the
  user-data address (block start + 6) or `0` on out-of-memory.
- `_PLSW_FREEMAIN(addr, n)` validates the alloc-magic sentinel
  (catches double-free + invalid-pointer), validates `n` matches
  the recorded block size (catches caller bookkeeping bugs),
  then sorted-inserts into the free list and forward-coalesces
  with the immediate-next free block if their address ranges
  abut.

**Forward coalesce only.** Backward coalesce (merge with the
immediate-previous free block) is deferred. Without it, a
sequence of allocations followed by frees in *forward order*
leaves N separate free blocks; freeing in *reverse order*
chain-coalesces them all. This is fine for SNOBOL4's expected
allocation pattern (LIFO-ish per pattern-match attempt) but
worth revisiting if profiling shows it matters.

Why free-list rather than bump or slab:

| Backend | Per-cell `?FREEMAIN` works? | Code size | Per-block overhead | Verdict |
|---|---|---|---|---|
| Bump-only | No (FREEMAIN is a no-op) | ~30 lines | 0 bytes | Doesn't satisfy the `_PLSW_FREEMAIN` contract; consumers needing real free have to roll their own anyway |
| Free-list | Yes | ~120 lines of `.msw` | 6 bytes/block | **Chosen and shipped in v1.** Honors both procedures' contracts at modest cost. |
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

1. **dcpls/`pr/storage-macros`** ✅ — shipped: `include/_plsw_storage.msw`
   with the free-list backend + `_PLSW_GETMAIN`/`_PLSW_FREEMAIN`
   procedures. Reg-rs cases `plsw_storage_*` cover the
   implementation.
2. **dcpls/`pr/getmain-freemain-macros`** ✅ — shipped: PL/X-style
   `?GETMAIN`/`?FREEMAIN` macros wrapping the procedures, plus
   the source-template MACRODEF mechanism (`{KEY}` placeholders
   in the body, body re-fed to the lexer/parser at expansion).
3. **dcsno/`pr/storage-allocation-runtime`** — SNOBOL4 builds its
   region/mark/reclaim runtime over PL/SW's primitives. See
   `tools/briefs/dcsno-storage-allocation-runtime.md`.
4. **dcpls/`pr/macro-preprocessor` (future, when needed)** —
   extend the source-template mechanism with `%IF`/`%ELSE`
   inside templates (so `RC` can become OPTIONAL on `?GETMAIN`),
   `%DO` loops, and nested `?MACRO` calls with finite recursion
   depth (PL/X has 255 levels by default; we'd cap at 32).
   Implementation choices in step 001 of the macros saga were
   structured to leave this path open.
5. **dcpls/`pr/storage-backward-coalesce` (future, only if
   profiled need)** — extend the free-list to coalesce with the
   immediate-previous free block on free, in addition to forward.
6. (Future) Other consumers (Prolog, etc.) layer their policy
   the same way.

## IBM heritage

The `?GETMAIN` / `?FREEMAIN` spelling and the
"`LEN` required on free" semantics come from IBM OS/MVS / OS/390:

```
GETMAIN  RU,LV=DATALEN,A=(R1)
FREEMAIN RU,LV=DATALEN,A=(R1)
```

Both `GETMAIN` and `FREEMAIN` required the length value (`LV=`)
because the OS/360-lineage allocator did not always store size in
an in-band header — it used `GQE`/`FQE` chains stored *outside*
the user region. Callers had to remember the length they
allocated. CICS later relaxed this (CICS tracked allocations
itself).

PL/SW *does* keep an in-band header (`_PLSW_BLOCK_SIZE` is
recorded), so we *could* drop the `LEN` parameter on free. We
keep it for two reasons: (1) PL/X spelling continuity, and
(2) the cross-check between caller-supplied `LEN` and the in-band
size catches a real class of bugs (wrong size remembered, header
corruption).

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
