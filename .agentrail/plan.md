# Test split (Phase 1 of shrink-lgo)

Per `docs/shrink-lgo-size.md` Phase 1. Move ~6,800 lines of test
code (46 `test_*` functions + `run_suite()` + the 'a'/numeric
branches of `main()`) out of `src/main.c` and into a separate
test driver. Build two `.lgo`s:

* `build/plsw.lgo` — production compiler (compile mode + REPL).
  Goal: shrink from 1.74 MB to under ~900 KB.
* `build/plsw_test.lgo` — test runner. Same headers, same
  compiler internals, but only test code in its top-level.

`just test` (reg-rs) does NOT use the in-binary test suites
(driver.sh + pipeline.sh use production `plsw.lgo` to compile
`.plsw` fixtures). So the split is functionally invisible to
`just test`. The only thing the test split changes is what the
production `.lgo` carries.

## Constraint discovered during prep

tc24r accepts `-D` on its CLI but silently ignores it for
`#ifdef`. Source-level `#define` works; CLI doesn't. So the
split must be done with two separate source files, not a single
`#ifdef BUILD_TESTS` switch. (Reported to tc24r is out of scope.)

## Approach

* New file `src/test_main.c` — `#include`s the same compiler
  headers as `src/main.c`, contains the 46 `test_*` functions,
  the `run_suite(int)` dispatcher, and a small `main()` that
  reads the suite # / "a" / "r" and dispatches.
* `src/main.c` shrinks to: includes, the source-read helpers
  (`read_line_term`, `read_file_content`, `read_source`,
  `read_compile_input`, `inc_buf_alloc`), `src_buf` /
  `inc_buf`, and a `main()` with only compile mode + REPL.
* `justfile`:
  - `build` still builds `plsw.s` from `main.c`.
  - New `build-test` target builds `plsw_test.s` from
    `test_main.c`, then `plsw_test.lgo`.
  - `test` keeps depending on `build-lgo` (production), since
    that's what reg-rs needs. A manual `just smoke-test` (or
    similar) can run the in-binary tests via plsw_test.lgo if
    needed for diagnostics.

## Tests

1. `just clean && just build-lgo` — must succeed and produce a
   smaller `plsw.lgo`.
2. `just build-test` — must succeed and produce `plsw_test.lgo`
   that runs the in-binary suites correctly.
3. `just test` — all 15 reg-rs tests still green.
4. CHANGES.md gets a new rebuild-trigger entry with the new SHA.

## Steps

1. **test-split**. Single step:
   - Create `src/test_main.c` with all test code + a test-only
     `main()`.
   - Strip `src/main.c` to production-only.
   - Add `build-test` / `build-test-lgo` targets to `justfile`.
   - Verify both binaries build.
   - Run `just test`; confirm 15/15 green.
   - Capture new `plsw.lgo` SHA + size; add CHANGES.md entry.
   - Commit + complete + mark-pr.

## What does NOT go in this PR

* No allocator change (Phase 2's chunk allocator).
* No buffer migration (Phase 3).
* No lint rule (Phase 5).
* No tc24r change.
