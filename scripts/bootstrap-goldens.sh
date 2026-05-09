#!/usr/bin/env bash
# scripts/bootstrap-goldens.sh -- (re)create reg-rs golden files
# from the current toolchain's output.
#
# IMPORTANT: golden files are authoritative ONLY after operator
# review. Reg-rs captures whatever the command produced, including
# captured-while-broken state -- a demo that fails to compile
# yields a golden with exit_code=1 and empty .out, which then
# "passes" forever because the broken behavior is reproducible.
# Reading every captured .rgt and .out before committing is the
# operator's responsibility, not the tool's.
#
# This script catches the easy case (non-zero exit code) and
# refuses to silently bless it: if any case has exit_code != 0
# after capture, the run aborts non-zero and prints a list. Pass
# --allow-failures to override (e.g. when a demo's expected
# behavior really is to exit non-zero -- rare; document why in
# docs/testing.md if so).
#
# After bootstrap, commit the resulting reg-rs/plsw_*.{rgt,out,err}
# triples to git -- reg-rs compares stdout to .out, stderr to
# .err, and exit code to the value in .rgt. The .tdb files are
# reg-rs's internal SQLite index, regenerated from the triples
# at run time, and are gitignored.
#
# Prerequisites:
#   * `just build-lgo` produces build/plsw.lgo (handled below).
#   * cor24-asm and cor24-emu on PATH.
#   * reg-rs on PATH.

set -euo pipefail

ALLOW_FAILURES=0
for arg in "$@"; do
    case "$arg" in
        --allow-failures) ALLOW_FAILURES=1 ;;
        *) echo "bootstrap-goldens: unknown arg '$arg'" >&2; exit 2 ;;
    esac
done

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

failed=()

for c in "${cases[@]}"; do
    echo "=== bootstrap: plsw_${c} ===" >&2
    reg-rs create -t "plsw_${c}" -c "./tests/driver.sh ${c}" >/dev/null

    rgt="$repo/reg-rs/plsw_${c}.rgt"
    out="$repo/reg-rs/plsw_${c}.out"
    if [ ! -f "$rgt" ]; then
        echo "  WARN: $rgt missing after create" >&2
        failed+=("plsw_${c} (no .rgt)")
        continue
    fi
    rc=$(awk -F'= *' '/^exit_code/{print $2}' "$rgt" | tr -d '[:space:]')
    out_lines=0
    if [ -f "$out" ]; then
        out_lines=$(wc -l < "$out" | tr -d '[:space:]')
    fi
    echo "  exit_code=${rc}  stdout_lines=${out_lines}" >&2

    if [ "${rc:-0}" != "0" ]; then
        failed+=("plsw_${c} (exit_code=${rc})")
    fi
done

echo >&2
echo "================================================================" >&2
echo "REVIEW BEFORE COMMITTING" >&2
echo "================================================================" >&2
echo "Open each reg-rs/plsw_*.{rgt,out,err} triple and confirm:" >&2
echo "  * .rgt has exit_code = 0 (unless intentionally non-zero)" >&2
echo "  * .out is non-empty and contains the expected program output" >&2
echo "  * .err's diagnostics look clean (no unexpected stack traces)" >&2
echo "Reg-rs will faithfully reproduce whatever was captured here," >&2
echo "including a demo that's broken right now. The bootstrap can't" >&2
echo "tell intent from regression. That's the operator's job." >&2
echo >&2

if [ ${#failed[@]} -gt 0 ]; then
    echo "Captured non-zero exit codes:" >&2
    for f in "${failed[@]}"; do
        echo "  - $f" >&2
    done
    if [ "$ALLOW_FAILURES" -eq 0 ]; then
        echo >&2
        echo "Refusing to declare bootstrap successful. Investigate the" >&2
        echo "failures above; likely a demo regression. Pass --allow-failures" >&2
        echo "if a non-zero exit really is the intended golden state." >&2
        exit 1
    else
        echo "(--allow-failures: declared successful anyway.)" >&2
    fi
fi

echo "Bootstrapped ${#cases[@]} golden(s) under reg-rs/. Commit:" >&2
echo "  git add reg-rs/plsw_*.rgt reg-rs/plsw_*.out reg-rs/plsw_*.err" >&2
