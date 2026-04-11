# PL/SW Vendored Toolchain Plan

Plan for stabilizing the PL/SW build dependencies on the COR24
cross-toolchain (C compiler, assembler, emulator) by vendoring
versioned binaries under `./vendor/` with version-controlled
metadata. This decouples PL/SW from upstream HEAD and from
whatever the user has system-installed via `sw-install`.

## Motivation

Today PL/SW invokes `tc24r` and `cor24-run` from sibling working
directories at HEAD:

- `~/github/sw-vibe-coding/tc24r/` (C compiler binary)
- `~/github/sw-embed/sw-cor24-emulator/` (assembler + emulator)
- `~/github/sw-embed/sw-cor24-tinyc/` (Rust source for tc24r)

This means any upstream change can silently break the PL/SW build
with no warning, no rollback path, and no record of which version
of each tool was actually exercised by a given PL/SW commit.
Migration of upstream changes is uncontrolled.

The goal is **controlled adoption**: PL/SW pins specific versions
of each tool, fresh checkouts can rebuild the exact toolchain,
upgrades are deliberate one-line commits, and the system-installed
tools (under `~/.local/softwarewrighter/bin/`) cannot interfere.

## Naming convention

| Today (cross) | Future (native, far away) | Role |
|---|---|---|
| `sw-cx24` | `sw-c24` | C compiler targeting COR24 |
| `sw-asx24` | `sw-as24` | Assembler targeting COR24 |
| `sw-em24` | (no native variant — emulator is host-side) | COR24 emulator |

The `x` infix means "cross" — runs on the host, produces or
operates on COR24 artifacts. The non-`x` names are reserved for
self-hosted variants that run on COR24 itself, when those exist.
The vendor scheme already supports both coexisting under separate
`vendor/<name>/` trees; no migration needed when self-hosted
variants ship.

Repos and binaries share the same name. Some current repos will
need renaming as part of the cutover.

## Directory layout

```
sw-cor24-plsw/
├── .gitignore                              # adds: vendor/*/*/bin/*
├── vendor/
│   ├── active.env                          # tracked, single source of truth
│   ├── sw-cx24/
│   │   └── v0.1.0/
│   │       ├── bin/sw-cx24                 # GITIGNORED (built locally)
│   │       ├── docs/                       # tracked (upstream docs snapshot)
│   │       ├── includes/                   # tracked (C headers needed at compile time)
│   │       └── version.json                # tracked (manifest)
│   ├── sw-asx24/
│   │   └── v0.1.0/
│   │       ├── bin/sw-asx24                # GITIGNORED
│   │       ├── docs/
│   │       └── version.json
│   └── sw-em24/
│       └── v0.1.0/
│           ├── bin/sw-em24                 # GITIGNORED
│           ├── docs/
│           └── version.json
├── scripts/
│   └── vendor-fetch.sh
└── docs/
    └── vendor-plan.md                      # this file
```

### Tracked vs. gitignored

- **Tracked**: `version.json`, `docs/`, `includes/`, `active.env`,
  `vendor-fetch.sh`. Everything except the binary itself.
- **Gitignored**: `bin/*`. Binaries are built locally from the
  upstream commit recorded in `version.json`. Reproducibility is
  enforced via SHA-256.

This means a fresh clone has the directory skeleton, the manifests,
the headers, and the upstream docs — but **no executables**. The
first build runs `./scripts/vendor-fetch.sh` to materialize them.

## `version.json` (manifest)

JSON, one per versioned directory. Captures everything needed to
rebuild the binary identically and verify it after the fact.

```json
{
  "name": "sw-cx24",
  "version": "v0.1.0",
  "description": "C cross-compiler targeting COR24",
  "repo": "git@github.com:sw-embed/sw-cx24.git",
  "repo_path_local": "../sw-cx24",
  "commit": "a1b2c3d4e5f6789abcdef0123456789012345678",
  "tag": "v0.1.0",
  "build_cmd": "cargo build --release",
  "binary_src": "target/release/sw-cx24",
  "platforms": {
    "darwin-arm64": {
      "sha256": "TBD",
      "size": 0
    }
  },
  "recorded_at": "TBD",
  "recorded_by": "vendor-fetch.sh --record"
}
```

Field notes:

- `repo` — canonical git URL, used for fresh clones if no local
  copy is available.
- `repo_path_local` — relative path from PL/SW root to a sibling
  working directory, used as a fast path when the user already has
  the upstream repo cloned.
- `commit` — full SHA. Authoritative pin. `vendor-fetch.sh` checks
  this commit out before building, regardless of upstream HEAD.
- `tag` — informational mirror of `commit` (e.g., `v0.1.0`). Tags
  are mutable; `commit` is the actual pin.
- `platforms.<platform>.sha256` — verified after every build.
  Starts as `"TBD"` until the first `--record` run fills it in.
  Mismatch is a hard failure.
- `recorded_at` / `recorded_by` — provenance, helpful for audits.

## `vendor/active.env`

Single source of truth for "which version is currently active" of
each tool. Tracked. `justfile` loads it via `set dotenv-load`;
`vendor-fetch.sh` sources it; future helper scripts read it.

```sh
# vendor/active.env
SW_CX24_VERSION=v0.1.0
SW_ASX24_VERSION=v0.1.0
SW_EM24_VERSION=v0.1.0
```

Bumping a version is one line in this file plus a new versioned
subdirectory under `vendor/<tool>/`. Old versions stay around for
fast rollback.

## `scripts/vendor-fetch.sh`

Single script with two modes.

### Verify mode (default)

Run on every fresh clone or after pulling new commits to PL/SW.

For each tool listed in `vendor/active.env`:

1. Read `vendor/<tool>/<version>/version.json`.
2. If `bin/<tool>` exists and its SHA-256 matches the platform
   entry → skip, already good.
3. Else, look for the upstream repo:
   - First try `repo_path_local` (fast path for developers who
     already have it cloned).
   - Else `git clone --depth=1 <repo>` into a temp dir.
4. `git checkout <commit>` — never builds at HEAD.
5. Run `build_cmd`.
6. Copy `binary_src` to `vendor/<tool>/<version>/bin/<tool>`.
7. Recompute SHA-256, compare to manifest. **Mismatch is a hard
   error** — script exits non-zero with the expected vs. actual
   hash so the user can investigate (was the build deterministic?
   was the manifest stale?).
8. (Re)copy `docs/` and `includes/` from upstream into the
   versioned dir, since those are tracked and shouldn't drift.

### Record mode (`--record <tool>`)

For first-time vendoring of a new tool/version, when no SHA-256 is
known yet.

Same as verify mode, except:

- Step 7 inverts: rather than verifying the SHA, **write** it into
  `version.json` along with `recorded_at` and `recorded_by`.
- Refuses to overwrite an existing non-`TBD` SHA without
  `--force`, so an accidental record can't silently replace a
  pinned hash.
- After recording, the user reviews `git diff vendor/<tool>/<ver>/
  version.json` and commits.

### Critical guarantee

**The build always happens at the recorded commit.** This is what
makes "control when to migrate forward or fall back" possible:
the manifest's `commit` field is authoritative, and the script
will not build at upstream HEAD even if the user passes a
local-path repo that's currently checked out somewhere else.

## `justfile` integration

```just
set dotenv-load := true
set dotenv-path := "vendor/active.env"

sw_cx24  := "vendor/sw-cx24/" + env_var("SW_CX24_VERSION") + "/bin/sw-cx24"
sw_asx24 := "vendor/sw-asx24/" + env_var("SW_ASX24_VERSION") + "/bin/sw-asx24"
sw_em24  := "vendor/sw-em24/" + env_var("SW_EM24_VERSION") + "/bin/sw-em24"

cx24_includes := "vendor/sw-cx24/" + env_var("SW_CX24_VERSION") + "/includes"

build:
    {{sw_cx24}} src/main.c -o build/plsw.s -I {{cx24_includes}} -I src

run: build
    {{sw_em24}} --run build/plsw.s --terminal --speed 0
```

PATH is **never modified**. Vendored binaries are always invoked
by absolute relative path, so a `~/.local/softwarewrighter/bin/
sw-cx24` installed by `sw-install` cannot accidentally shadow the
vendored copy. The two coexist with zero interference.

## Upgrade workflow

```bash
# Drop in a new version.
mkdir -p vendor/sw-cx24/v0.1.1/{docs,includes}
cp vendor/sw-cx24/v0.1.0/version.json vendor/sw-cx24/v0.1.1/version.json
# Edit version.json: bump version, commit, tag; reset sha256 to "TBD".

./scripts/vendor-fetch.sh --record sw-cx24    # builds at new commit, records SHA
just test                                      # verify nothing broke

# If green:
sed -i '' 's/^SW_CX24_VERSION=.*/SW_CX24_VERSION=v0.1.1/' vendor/active.env
git add vendor/sw-cx24/v0.1.1 vendor/active.env
git commit -m "vendor(sw-cx24): bump to v0.1.1 (upstream a1b2c3d)"

# If broken: just don't update active.env. v0.1.0 stays in use.
# Old versioned dir stays around for instant rollback if a
# subsequent bump turns out to be bad.
```

The bump is one commit, fully reversible by reverting it. Old
versions can be deleted manually once a new version has been in
production for a while, but they're cheap to keep.

## Cold-clone bootstrap

For someone (or some agent) who just `git clone`'d this repo:

```bash
git clone <plsw-repo> sw-cor24-plsw
cd sw-cor24-plsw
./scripts/vendor-fetch.sh    # materializes vendor/*/*/bin/* from manifests
just build
```

If `repo_path_local` resolves (relative paths to sibling clones),
that's the fast path. Otherwise the script clones each upstream
fresh. Either way, the build is fully reproducible from the
committed manifests.

## Saga sequencing

This work is structured enough to deserve its own saga:
**`plsw-vendor-toolchain`**, with these steps:

1. **Skeleton + script**: `docs/vendor-plan.md` (this file),
   `vendor/.gitignore`, `vendor/active.env`,
   `scripts/vendor-fetch.sh` (verify + `--record` modes), an empty
   `vendor/sw-cx24/v0.1.0/version.json` skeleton with `sha256:
   "TBD"`. No real binaries yet. Pure scaffolding.
2. **Vendor sw-cx24**: pin a stable upstream commit, run
   `vendor-fetch.sh --record sw-cx24`, copy `includes/` and
   `docs/`, verify the resulting binary builds PL/SW byte-identical
   to the current `tc24r`-built version. May involve upstream
   renaming work in the C compiler repo first.
3. **Vendor sw-asx24**: same workflow for the cross-assembler.
   Likely needs upstream split from the current
   `sw-cor24-emulator` repo, since assembler and emulator are
   currently one binary.
4. **Vendor sw-em24**: same workflow for the emulator.
5. **Cut over justfile + CLAUDE.md**: switch all build paths to
   the vendor tree; rewrite "Stable toolchain references" in
   CLAUDE.md to point at `vendor/`. Verify `just build`,
   `just test`, `just pipeline examples/select_demo.plsw` all
   green.
6. **Bootstrap docs + cold-clone test**: README "First-time setup"
   section. Test the cold-clone scenario by removing
   `vendor/*/*/bin/*` and rerunning `vendor-fetch.sh; just build`.
   Document the upgrade workflow.

After this saga is archived, start a fresh maintenance saga
(timestamped, e.g. `plsw-maint-20260411T1430`) for ongoing fixes.

## Open considerations / future work

- **Multi-platform builds.** The `platforms` map in `version.json`
  supports multiple SHA-256 entries, but step 1 only seeds
  `darwin-arm64`. Adding `linux-x86_64` etc. is a one-time
  per-platform record-mode invocation when a developer on that
  platform first builds.
- **Native (non-cross) variants.** When `sw-c24` / `sw-as24` ship
  someday, they live under `vendor/sw-c24/` / `vendor/sw-as24/`
  alongside the cross variants. PL/SW could even run both during
  an A/B-test era. No scheme changes needed.
- **Verification on every build vs. on-demand.** Verify mode is
  cheap (read+hash), but running it on every `just build` would
  slow down inner loops. Recommendation: `just build` skips it,
  `just bootstrap` and CI run it explicitly. Open for tweaking.
- **Garbage collection of old vendored versions.** No automation
  for this. Manual `rm -rf vendor/<tool>/v<old>/` when confident
  the version is no longer needed for rollback.
- **Submodule vs. clone.** Considered git submodules for the
  upstream repos but rejected: submodules force every PL/SW
  contributor to clone every upstream tool, whereas vendoring just
  the binary + headers keeps PL/SW lean. The `repo_path_local`
  fast path keeps developer ergonomics good.
