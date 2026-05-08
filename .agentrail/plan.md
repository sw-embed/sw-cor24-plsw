# PL/SW Test Harness Saga

The bootstrap-toolchain saga shipped without an automated way to
verify the migration didn't regress observable behavior. The brief's
verification gauntlet was manual, the .plsw examples have no golden
outputs, and the linker demos have a Linux portability bug
(`stat -f%z` is BSD/macOS) that means they've never actually run on
this dev's environment. This saga closes those gaps.

## Goal

After this saga: `just test` runs an automated regression suite
that exercises (a) every `.plsw` example through the
`pipeline.sh` flow, (b) both linker test scripts end-to-end. Each
case has a golden output checked into the repo. A toolchain
regression in any of `tc24r`, `cor24-asm`, or `cor24-emu` shows up
as a diff in CI output, not as silently-passing test runs.

The harness uses `reg-rs` -- the golden-output regression tool
already in use across `sw-cor24-*` (per sw-cor24-basic and others)
-- rather than a one-off diff script, so PL/SW slots into the
existing devgroup convention.

## Steps

1. **plsw-test-harness**. Single step, the full delivery:
   - Fix `stat -f%z` -> portable `wc -c <` (or `stat -c%s`) in
     `components/linker/tests/demo-fixup.sh` and
     `demo-plsw-modular.sh`.
   - Add `tests/` directory with per-example golden files
     (`tests/expected/<name>.{out,rgt}` per the reg-rs convention).
   - Add `scripts/test.sh` that wraps `reg-rs run -p plsw_ --parallel`.
   - Add a `tests/cases/` directory for any fixed UART input
     sequences (none needed for the existing examples; placeholder).
   - Add a `just test` recipe that runs both the linker demos and
     the reg-rs example suite.
   - Add a `just test-bootstrap-goldens` recipe (or doc step) that
     regenerates golden files from the current toolchain output --
     used once when the golden baseline is first created and
     manually re-run when an intentional behavior change lands.
   - Document the harness, the bootstrap-goldens workflow, and the
     known tc24r-vs-main.c blocker (offset 165286 parse error) in
     README and a new `docs/testing.md`.

   Verification:
   - `./components/linker/tests/demo-fixup.sh` passes.
   - `./components/linker/tests/demo-plsw-modular.sh` passes (or is
     marked `SKIP` while tc24r blocker is open).
   - `./scripts/test.sh` exits 0 (or surfaces the tc24r block as a
     skip; never silently green when goldens are missing).

## Known blocker (out of scope for this saga)

`tc24r src/main.c` fails with "expected RBracket, got Star" at
offset 165286 (line 5849, "} else {"). The error is consistent
across many historical commits of `main.c`, suggesting the regression
is in the currently-installed `tc24r` (May 3 build), not in PL/SW
source. Until that lands a fix, `just build` and `just build-lgo`
can't produce the .lgo, and the .plsw example goldens can't be
bootstrapped. This saga writes the harness so it's ready to light
up the moment tc24r is unblocked.

A separate brief should go to dcxtc with: tc24r 2026-05-03 binary
panics on PL/SW's `src/main.c`; need either a tc24r fix or a
diagnosis of which main.c construct trips it.

## Rules

- No PL/SW compiler logic changes.
- No changes to `cor24-asm`, `cor24-emu`, or `tc24r`.
- Goldens are checked-in artifacts, not generated at test time. The
  bootstrap recipe is opt-in.
- Don't fix the tc24r issue here; surface it as a separate brief.
- `pr/plsw-test-harness` is the branch name; rename
  `feat/plsw-test-harness` via `dg-mark-pr` at the end.
