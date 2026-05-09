# PL/SW Storage Macros Saga

Implements `?GETMAIN`/`?FREEMAIN` and the underlying free-list
allocator per the design in `docs/storage-allocation.md`.
Single-file inline pattern (`include/_plsw_storage.msw`) so that
`%INCLUDE _plsw_storage;` text-concats macros + impl + heap into
the entry module. Multi-module consumers (SNOBOL4) use the
`PLSW_STORAGE_HEADERS_ONLY` guard to include only the macros in
non-entry modules. The `_plsw_` filename prefix reserves
`storage.{msw,plsw}` for application use (e.g., SNOBOL4's own
mark/reclaim runtime).

## Goal

After this saga: `%INCLUDE _plsw_storage;` brings `?GETMAIN` /
`?FREEMAIN` macros and the `_PLSW_GETMAIN` / `_PLSW_FREEMAIN`
procedures into a PL/SW program, plus a 64 KB default heap
(overridable via `%DEFINE PLSW_HEAP_SIZE`). The reg-rs test
suite covers basic alloc/free, coalesce-on-free, OOM, double-free
detection, and size-mismatch detection.

## Steps

1. **storage-macros**. Single step:
   - Implement `include/_plsw_storage.msw`: `%DEFINE
     PLSW_HEAP_SIZE 65536` default; `_PLSW_HEAP_BUF` byte array;
     `_PLSW_FREE_HEAD` and init flag; `_PLSW_GETMAIN(SIZE)` and
     `_PLSW_FREEMAIN(ADDR, LEN)` procedures with first-fit
     allocation, forward-coalesce on free, and double-free
     detection via `next == 0xFFFFFF` allocated-magic; MACRODEFs
     for `?GETMAIN` and `?FREEMAIN` thin-wrapping the procs.
   - `PLSW_STORAGE_HEADERS_ONLY` guard so non-entry modules can
     get only the macros + EXTERNAL decls.
   - Add demos: `examples/storage_basic.plsw`,
     `storage_coalesce.plsw`, `storage_oom.plsw`,
     `storage_double_free.plsw`, `storage_size_mismatch.plsw`.
   - Wire each into `tests/driver.sh` and
     `scripts/bootstrap-goldens.sh`.
   - Bootstrap reg-rs goldens for the 5 new cases; verify
     `just test` passes 15/15 (the existing 10 + new 5).
   - Update `docs/storage-allocation.md` to reflect the
     implementation choice (single-file inline + headers-only
     guard) and note implementation details (coalesce policy,
     magic value for double-free detection, lazy init).

## Rules

- No PL/SW compiler logic changes.
- No SUBPOOL clause (per design doc; never needed for COR24).
- No backward-coalesce in v1 (documented as future).
- Free-list backend only (no bump/slab alternates).
