# Parallel Test Isolation Saga

`just test` runs the reg-rs suite with `--parallel`, but on
macOS it fails non-deterministically: different cases fail on
each run, with no actual code change between runs. Diagnosis:
parallel pipelines share file paths in two places.

1. **`mktemp /tmp/plsw-XXXXXX.s`** — the `XXXXXX` template is
   followed by a literal `.s` suffix. BSD `mktemp` (macOS
   default) substitutes only trailing X's; whether it tolerates
   non-trailing X's varies. Even when it works, two parallel
   processes on different hosts can synthesize the same
   filename if the random source happens to collide. Subtle.

2. **`pipeline.sh`'s conditional rebuild of `build/plsw.lgo`**
   at line 33: if `.s` is newer than `.lgo`, parallel pipelines
   can each fire `cor24-asm "$COMPILER_ASM" -o "$COMPILER_LGO"`
   and race on writing the same file. `just test`'s `build-lgo`
   dependency normally prevents triggering this, but timestamp
   resolution or unusual invocation patterns can make it fire.

## Goal

After this saga: `just test` runs reg-rs in parallel
deterministically. Multiple back-to-back runs produce the same
result with no random failures.

## Steps

1. **parallel-test-isolation**. Single step:
   - `scripts/pipeline.sh`: replace the per-file `mktemp
     /tmp/plsw-XXXXXX.{s,lgo}` pattern with a single
     `mktemp -d /tmp/plsw-XXXXXX` per invocation. Put
     deterministically-named files inside (`program.s`,
     `program.lgo`). `mktemp -d` works the same on Linux and
     macOS; deterministic names mean the only randomness is
     the directory, which mktemp guarantees unique.
   - `scripts/pipeline.sh`: replace the conditional auto-rebuild
     of `build/plsw.lgo` with a clean error message when the
     `.lgo` is missing or stale. Force callers to run
     `just build-lgo` (or equivalent) first. `just test`'s
     existing `build-lgo` dependency handles this transparently;
     ad-hoc CLI users get a clear "run `just build-lgo` first"
     error.
   - `scripts/pipeline-dump.sh`: same `mktemp -d` treatment for
     any intermediate scratch (its `build/<name>.*` outputs are
     intentional persistent artifacts, not temp files, and stay
     as-is).
   - Verify: clean rebuild + `just test` 5 times in a row, all
     should be 15/15 green with identical output. (Heuristic --
     can't fully reproduce mac-side flake, but eliminating the
     known shared-write paths is the surgical fix.)
   - Update `docs/testing.md` with a "Parallel-safety" note
     mentioning the scratch-dir pattern and the build-lgo-first
     prerequisite.

## Rules

- Behavior on success unchanged: same exit codes, same stdout,
  same stderr (modulo the diagnostic "missing build/plsw.lgo"
  message, which only fires when `.lgo` is genuinely missing).
- No goldens need re-bootstrapping (program output is identical).
