#!/usr/bin/env bash
# tests/driver.sh -- per-case dispatch into scripts/pipeline.sh.
# Used by reg-rs tests (reg-rs/plsw_<case>.rgt) and by anyone
# wanting to reproduce a single test by hand.
#
# Usage: tests/driver.sh <case>
#   case := name of an examples/<case>.plsw fixture
#
# The driver knows which .msw includes each example needs and
# emits them on the pipeline.sh command line in the right order.
# Stdout is the program's UART output (the same thing pipeline.sh
# would print). Anything diagnostic from pipeline.sh goes to
# stderr and is therefore captured by reg-rs but not part of the
# golden text.

set -euo pipefail

case_name="${1:?usage: $0 <case>}"
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/.." && pwd)"

pipeline="$repo/scripts/pipeline.sh"
ex="$repo/examples"
inc="$repo/include"

case "$case_name" in
    hello)              args=("$ex/hello.plsw") ;;
    led)                args=("$ex/led.plsw") ;;
    loop)               args=("$ex/loop.plsw") ;;
    record)             args=("$ex/record.plsw") ;;
    define)             args=("$ex/define.plsw") ;;
    select_demo)        args=("$ex/select_demo.plsw") ;;
    select_nested)      args=("$ex/select_nested.plsw") ;;
    macro)              args=("$ex/system.msw" "$ex/macro.plsw") ;;
    hello_macro)        args=("$ex/greet.msw" "$ex/hello_macro.plsw") ;;
    chain)              args=("$inc/cvt.msw" "$inc/ascb.msw" "$inc/asxb.msw"
                              "$inc/tcb.msw" "$ex/chain.plsw") ;;
    *)
        echo "driver.sh: unknown case '$case_name'" >&2
        echo "available cases: hello led loop record define select_demo select_nested macro hello_macro chain" >&2
        exit 2
        ;;
esac

exec "$pipeline" "${args[@]}"
