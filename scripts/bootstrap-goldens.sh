#!/usr/bin/env bash
# scripts/bootstrap-goldens.sh -- (re)create reg-rs golden files
# from the current toolchain's output.
#
# Run this once when first creating the golden baseline, or
# manually after an intentional behavior change lands. After
# bootstrap, commit the resulting reg-rs/plsw_*.{rgt,out,err}
# triples to git -- reg-rs compares stdout to .out, stderr to
# .err, and exit code to the value in .rgt. The .tdb files are
# reg-rs's internal SQLite index, regenerated from the triples
# at run time, and are gitignored.
#
# Prerequisites:
#   * `just build-lgo` produces build/plsw.lgo (currently blocked
#     on dcxtc/pr/array-size-expressions and
#     dcxtc/pr/string-literal-concatenation; see docs/testing.md).
#   * cor24-asm and cor24-emu on PATH.
#   * reg-rs on PATH.
#
# Each test is created via `reg-rs create -t plsw_<case> -c
# 'tests/driver.sh <case>'`. The command is run, its stdout +
# stderr + exit code captured, and a .rgt + .out pair written.

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/.." && pwd)"
cd "$repo"

export REG_RS_DATA_DIR="$repo/reg-rs"

cases=(
    hello
    led
    loop
    record
    define
    select_demo
    select_nested
    macro
    hello_macro
    chain
    storage_basic
    storage_coalesce
    storage_oom
    storage_double_free
    storage_size_mismatch
)

# Ensure the compiler .lgo exists before we start. Without it the
# pipeline.sh inside driver.sh would re-trigger a tc24r build per
# case, which is wasteful and obscures real failures.
if [ ! -f "$repo/build/plsw.lgo" ]; then
    echo "bootstrap-goldens: building build/plsw.lgo first..." >&2
    just build-lgo
fi

for c in "${cases[@]}"; do
    echo "=== bootstrap: plsw_${c} ===" >&2
    reg-rs create -t "plsw_${c}" -c "./tests/driver.sh ${c}"
done

echo >&2
echo "Bootstrapped ${#cases[@]} golden(s) under reg-rs/. Commit:" >&2
echo "  git add reg-rs/plsw_*.rgt reg-rs/plsw_*.out reg-rs/plsw_*.err" >&2
