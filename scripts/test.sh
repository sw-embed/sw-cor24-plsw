#!/usr/bin/env bash
# scripts/test.sh -- run the PL/SW reg-rs regression suite.
#
# Mirrors the convention used across sw-cor24-* siblings (see
# sw-cor24-basic/scripts/test.sh): every test name is prefixed
# with `plsw_` so a single `reg-rs run -p plsw_` invocation
# matches them all. Tests are stored under <repo>/reg-rs/, with
# the data dir explicitly pointed at that location so the suite
# is self-contained per-repo (rather than mixed into the user's
# default ~/.local/reg-rs/).
#
# Prerequisite: golden baselines must exist. If reg-rs reports
# "no tests matched", run `just test-bootstrap-goldens` first.
# That recipe is gated on a working `just build-lgo`, which today
# requires both dcxtc/pr/array-size-expressions and
# dcxtc/pr/string-literal-concatenation to land in tc24r. See
# docs/testing.md for the current blocker status.

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/.." && pwd)"

export REG_RS_DATA_DIR="$repo/reg-rs"
exec reg-rs run -p plsw_ --parallel "$@"
