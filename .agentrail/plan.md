# PL/SW Bootstrap Goldens Saga

The `plsw-test-harness` saga shipped scaffolding-only because the
PL/SW compiler couldn't build (tc24r limitations). Both tc24r
fixes have now landed, the new toolchain is on PATH, and `pl-sw`
+ `plsw.lgo` are installed in the shared toolchain location. This
saga turns `just test` into a real regression gate by capturing
committed goldens for every `.plsw` example.

## Goal

After this saga: `just test` runs all 10 reg-rs cases and they
all pass against committed `reg-rs/plsw_<case>.{rgt,out}` files.
From here on, any drift in compiler output surfaces as a test
failure.

## Steps

1. **bootstrap-goldens**. Single step, full delivery per
   `tools/briefs/dcpls-bootstrap-goldens.md`:
   - Trivial build polish: `mkdir -p build` in the justfile
     `build:` recipe so a fresh clone doesn't fail with "cannot
     write build/plsw.s".
   - Build cleanly end-to-end: `just clean && just build && just
     build-lgo`. Confirm `build/plsw.lgo` is non-trivial (>1MB).
   - `just test-bootstrap-goldens` to capture all 10 goldens.
   - `just test` to verify the gate is green.
   - Optional: `just test-linker` and capture the demo-fixup.sh
     symptom precisely in `docs/testing.md` for follow-up.
   - Commit the 20 new files (10 .rgt + 10 .out) plus the
     justfile polish.

## Rules

- No compiler logic changes. Goldens are captured as-is from
  the current plsw.lgo.
- No fixes to demo-fixup.sh garbled-UART issue (out of scope).
- No new test cases beyond the existing 10.
