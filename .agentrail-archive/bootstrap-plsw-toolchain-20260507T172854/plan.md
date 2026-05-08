# Bootstrap PL/SW Toolchain Saga

Make PL/SW shippable into the shared COR24 toolchain that mike's
forthcoming `tools/build-all` orchestrator will install. After
this saga, downstream consumers (sw-cor24-snobol4, prolog, oca,
basic, script) can find PL/SW artifacts on a stable path instead
of reaching into our `build/` via `$HOME` / sibling-relative paths.

The brief is at
`/disk1/github/softwarewrighter/devgroup/tools/briefs/dcpls-bootstrap-plsw-toolchain.md`
-- read it first; it is the authoritative reference for migration
mappings and verification steps. This plan is the saga arc.

## Goal

After this saga: `just build-lgo` produces `build/plsw.lgo` (the
canonical shippable artifact); every callsite of the deprecated
`cor24-run --run` and `cor24-run --assemble` flags has been
migrated to the new `cor24-asm` + `cor24-emu --lgo` pipeline; and
`just install-layer1` (or equivalent) stages `link24` and
`meta-gen` for the orchestrator to grab.

## Forcing event

`sw-cor24-emulator`'s `pr/remove-internal-assembler` saga landed,
removing `--run` and `--assemble` flags from the new `cor24-emu`
binary. Every existing build script in this repo that uses those
flags will break the moment mike installs the new emulator. The
new pipeline is `cor24-asm` (assemble) + `cor24-emu --lgo`
(execute) as separate steps.

## Steps

1. **bootstrap-toolchain**. The full brief, landed as one PR:
   - Add `just build-lgo` recipe producing `build/plsw.lgo`.
   - Migrate `justfile` (`run`, `run-input`, `test`),
     `scripts/pipeline.sh`, `scripts/pipeline-dump.sh`, and
     `components/linker/tests/demo-fixup.sh` +
     `demo-plsw-modular.sh` to the new pipeline.
   - Add `just install-layer1` staging `link24` + `meta-gen` to
     `dist/bin/` (gitignored) for the orchestrator.
   - Fix the hardcoded `tc24r_include` path in `justfile:4` from
     `sw-vibe-coding/tc24r/include` to
     `sw-embed/sw-cor24-x-tinyc/include`.
   - Update README to document the canonical artifact paths.
   - Run the full verification gauntlet from the brief.

After this saga is archived, downstream-consumer migration sagas
(in their own repos) can begin once mike's orchestrator installs
the shared artifacts.

## Rules

- No PL/SW compiler logic changes. Build-system migration only.
- No cross-repo updates. Document the new artifact paths so
  downstream repos can migrate on their own schedule.
- No installing into `work/bin/`. Mike does that post-relay.
- `pr/bootstrap-toolchain` is the branch name expected by the
  brief; rename `feat/bootstrap-toolchain` to that on completion.
