# PL/SW Vendor Toolchain Saga

Vendor the COR24 cross-toolchain (sw-cx24, sw-asx24, sw-em24)
under `./vendor/` so PL/SW pins specific versions of each tool
and adoption of upstream changes is deliberate.

Architectural design lives in `docs/vendor-plan.md` -- read it
first. This plan.md is the saga arc; the design doc is the
authoritative reference.

## Goal

After this saga: a fresh `git clone` of sw-cor24-plsw + one run
of `./scripts/vendor-fetch.sh` rebuilds the exact toolchain
binaries pinned by version.json manifests, with SHA-256
verification, and `just build` works without referencing any
sibling working directory or anything in `~/.local/`.

## Steps

1. **Skeleton + script**. Pure scaffolding, no real binaries:
   - `vendor/.gitignore` (ignores `*/v*/bin/*`)
   - `vendor/active.env` (committed, version pins)
   - `scripts/vendor-fetch.sh` (verify + `--record` modes)
   - `vendor/sw-cx24/v0.1.0/version.json` skeleton with TBD sha256
   - The `docs/vendor-plan.md` design doc gets folded into this
     step's commit (it was drafted at saga-init time)
   - No vendored binaries yet; script untested end-to-end

2. **Vendor sw-cx24**. Pin a stable upstream commit, run
   `vendor-fetch.sh --record sw-cx24`, copy `includes/` and
   `docs/`, verify the resulting binary builds PL/SW
   byte-identical to the current `tc24r`-built version.
   Possibly involves an upstream rename of the C compiler repo.

3. **Vendor sw-asx24**. Same workflow for the cross-assembler.
   Likely needs upstream split from `sw-cor24-emulator`, since
   assembler and emulator are currently one binary.

4. **Vendor sw-em24**. Same workflow for the emulator.

5. **Cut over justfile + CLAUDE.md**. Switch all build paths to
   the vendor tree; rewrite "Stable toolchain references" in
   CLAUDE.md to point at `vendor/`. Verify `just build`,
   `just test`, `just pipeline examples/select_demo.plsw` all
   green.

6. **Bootstrap docs + cold-clone test**. README "First-time
   setup" section. Test the cold-clone scenario by removing
   `vendor/*/*/bin/*` and rerunning `vendor-fetch.sh; just build`.
   Document the upgrade workflow.

After this saga is archived, start a timestamped maintenance
saga (e.g. `plsw-maint-20260411Thhmm`) for ongoing fixes.

## Rules

- Each step is one focused commit (or a small bundle).
- Steps 2-4 may also produce upstream commits in the toolchain
  repos -- note that explicitly in the step prompt.
- Don't expand scope. If a step uncovers something else broken,
  log it for a future maintenance step.
