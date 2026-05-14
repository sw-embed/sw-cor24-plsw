#!/usr/bin/env bash
# run-chunk-stress.sh -- Compile-only stress test for the AST chunk pool.
#
# Drives examples/chunk_stress.plsw through the production
# build/plsw.lgo and asserts the compile succeeds without "AST
# pool exhausted" / "ERROR:" markers. The test is *compile-only*:
# we don't assemble + run the generated .s because the chunk-stress
# fixture is synthetic (helper PROCs are never called, MAIN is
# trivial) and pipeline.sh's default -n 200M instruction budget is
# too tight for a fixture this large (real compile takes ~620M
# instructions). We use -n 2000M here, isolated to this one test.
#
# Existence rationale: this regression test would have caught the
# 2026-05-12 capacity regression (CHUNK_MAX=16 too small for
# real-world inputs). See:
#   - tools/briefs/dcpls-ast-chunk-capacity-and-test.md
#   - docs/memory-audit-2026-05-12.md
#   - examples/chunk_stress.plsw (header comment)
#
# Stdout is a single deterministic line so the reg-rs golden is
# stable. Anything diagnostic goes to stderr.

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/.." && pwd)"

LGO="$repo/build/plsw.lgo"
FIXTURE="$repo/examples/chunk_stress.plsw"

if [ ! -f "$LGO" ]; then
    echo "run-chunk-stress: missing $LGO (run 'just build-lgo' first)" >&2
    exit 1
fi
if [ ! -f "$FIXTURE" ]; then
    echo "run-chunk-stress: missing $FIXTURE" >&2
    exit 1
fi

# Build UART input (legacy mode -- no .msw to register).
INPUT=$(
    printf 'c\n'
    cat "$FIXTURE"
    printf '\x04'
)

OUT=$(cor24-emu --lgo "$LGO" -u "$INPUT" -n 2000000000 -t 600 --speed 0 -q 2>&1)

# Capacity-regression signals (any one is a fail).
if printf '%s\n' "$OUT" | grep -qE 'AST pool exhausted|chunk_alloc returned NULL|SYNTAX ERROR|STORAGE ERROR|compilation failed'; then
    echo "run-chunk-stress: FAIL -- capacity regression detected" >&2
    printf '%s\n' "$OUT" | grep -E 'ERROR|exhausted|failed' | head -10 >&2
    exit 1
fi

# Verify the compile actually finished (end-of-assembly marker
# present -- if cor24-emu hit -n / -t and the assembly was
# truncated, the marker is missing).
if ! printf '%s\n' "$OUT" | grep -q -- '--- end assembly ---'; then
    echo "run-chunk-stress: FAIL -- compile output truncated (--- end assembly --- not seen)" >&2
    printf '%s\n' "$OUT" | tail -10 >&2
    exit 1
fi

echo "chunk-stress: compile ok"
