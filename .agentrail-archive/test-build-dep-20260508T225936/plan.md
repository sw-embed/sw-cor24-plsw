# Test-Build Dependency Saga

`just test` was wired up without a `build-lgo` dependency, so on
a fresh clone (no `build/plsw.lgo` yet) all 15 reg-rs cases fail
with `Error: compiler not built (build/plsw.s)`. Latent bug
since the test harness saga.

## Goal

`just test` works from a fresh clone. Add the `build-lgo`
dependency and document the dependency chain in `docs/testing.md`.

## Steps

1. **test-build-dep**. Single step:
   - `justfile`: `test: build-lgo` (and `test-linker: build-lgo`
     if its demos need it).
   - Verify with a clean rebuild + simulated fresh-clone test.
   - Update `docs/testing.md` to mention the dependency.
