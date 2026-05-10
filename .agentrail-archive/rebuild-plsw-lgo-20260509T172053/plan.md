# Rebuild PL/SW LGO Saga

Per `tools/briefs/dcpls-rebuild-plsw-lgo.md`. The `.zero N`
codegen change shipped today (`c7e1262`, merged via
`pr/emit-zero-fill`), but `work/lib/cor24/plsw.lgo` is from
May 9 00:04 -- before the change landed. Until that artifact is
rebuilt, `pl-sw` invocations still emit per-byte `.byte 0` and
dcsno's `snobol4-runtime-split` step 2 stays blocked.

## Goal

Signal to mike that `plsw.lgo` should be rebuilt from current
`dev` (or `main` after promotion) and reinstalled to
`work/lib/cor24/plsw.lgo`. Build chain is `just clean && just
build-lgo` -- one command, deterministic, no source changes.

## Approach (option 2 from the brief: docs only)

The brief offers two shapes; I'm taking option 2 (doc the
rebuild command, mike rebuilds at relay) because it matches the
existing `build/` gitignored convention and avoids committing a
2.3 MB binary. Adding a CHANGES.md entry plus the agentrail
bookkeeping is the deliverable; the actual rebuild is mike's
post-relay action.

## Steps

1. **rebuild-plsw-lgo**. Single step:
   - Verify `c7e1262` is in current `dev` (`git log --oneline
     --grep='zero N' -1`).
   - Run `just clean && just build-lgo` from `dev`; confirm
     `build/plsw.lgo` exists and is non-trivial (>1 MB).
   - Run a quick probe: compile a `.plsw` with `DCL X(64) BYTE
     INIT(0);` and verify the emitted `.s` contains `.zero 64`.
   - Run `just test` (15/15 green) to confirm the new compiler
     doesn't regress any demo.
   - Add `CHANGES.md` (new file) recording the milestone: the
     codegen change shipped, this saga signals time to rebuild
     `plsw.lgo`. Include the exact rebuild command and the
     installation target.
   - Commit + complete + mark-pr. The pr/ branch's diff is the
     CHANGES.md plus the agentrail records -- a short signal,
     not a heavy artifact.
