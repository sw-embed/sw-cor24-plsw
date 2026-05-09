# PL/SW Storage Allocation Design Doc Saga

The SNOBOL4 interpreter (and other PL/SW consumers) needs heap
storage for many small short-lived allocations. PL/SW currently
has no built-in heap -- programs roll their own bump arena per
`examples/chain.plsw`. This saga documents the design for a
PL/SW-provided opt-in heap allocator and surfaces a brief to
dcsno so SNOBOL4 can plan its runtime accordingly.

The key layering decision: PL/SW provides only the raw alloc/free
primitives (`?GETMAIN`/`?FREEMAIN`); region/mark/reclaim/GC
semantics live in the consumer's runtime. This keeps PL/SW's
substrate language-neutral and reusable across consumers
(SNOBOL4, Prolog, future Fortran, etc.).

## Goal

After this saga: `docs/storage-allocation.md` documents the design
contract for `?GETMAIN`/`?FREEMAIN` and the layering rule that
higher-level storage policy belongs in consumers, not in PL/SW
macros. A brief in `tools/briefs/dcsno-storage-allocation-runtime.md`
points dcsno at the doc so SNOBOL4 can plan its mark/reclaim
runtime on top of the (still-to-be-implemented) PL/SW primitives.

## Steps

1. **storage-allocation-doc**. Single step:
   - Write `docs/storage-allocation.md` describing the layering,
     the macro contract (`?GETMAIN`/`?FREEMAIN` only), the
     `plsw_storage.msw` + `plsw_storage.plsw` two-file pattern
     for opt-in, the free-list backend choice, and the explicit
     non-goal that region/mark/GC live in consumers.
   - Drop a brief at
     `/disk1/.../tools/briefs/dcsno-storage-allocation-runtime.md`
     pointing dcsno to the design doc and outlining what their
     SNOBOL4 runtime saga will need to do.
   - Cross-link the brief from `docs/storage-allocation.md`.
   - This saga does NOT implement the PL/SW macros; that's a
     separate dcpls saga (`pr/storage-macros` or similar) cut
     after the design has been reviewed.

## Rules

- No `plsw_storage.msw` or `plsw_storage.plsw` source -- doc only.
- No SNOBOL4 changes -- the brief is the deliverable to dcsno,
  the implementation is dcsno's saga.
- Brief is agent-drafted; ownership inherits dcpls:devgroup;
  mike reviews at relay time.
