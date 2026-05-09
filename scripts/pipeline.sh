#!/bin/bash
# pipeline.sh -- End-to-end PL/SW compilation and execution pipeline
#
# Usage: ./scripts/pipeline.sh [macro.msw ...] program.plsw
#
# When .msw files are provided, uses the FILE:/SOURCE: protocol so the
# compiler's %INCLUDE mechanism resolves them by name. Without .msw files,
# uses legacy mode (raw source + EOT).
#
# Pipeline (post toolchain bootstrap):
#   .plsw  ->  cor24-emu --lgo build/plsw.lgo  ->  .s
#         (assembled in-place to a temp .lgo)  ->  cor24-emu --lgo
#         ->  program output
#
# Requires: cor24-asm, cor24-emu, the compiler assembly at build/plsw.s
# (`just build`), and the compiler .lgo at build/plsw.lgo (auto-built
# below if missing).

set -euo pipefail

COMPILER_ASM="${COMPILER_ASM:-build/plsw.s}"
COMPILER_LGO="${COMPILER_LGO:-build/plsw.lgo}"

if [ $# -lt 1 ]; then
    echo "Usage: $0 [macro.msw ...] program.plsw" >&2
    exit 1
fi

# Verify compiler artifacts exist. Don't auto-rebuild here:
# parallel reg-rs runs (e.g. via `just test`) can fire multiple
# pipeline.sh instances concurrently, and an auto-rebuild block
# would race on writing build/plsw.lgo. Force callers to do the
# build first (`just test` already does via the build-lgo dep).
if [ ! -f "$COMPILER_ASM" ]; then
    echo "Error: compiler not built ($COMPILER_ASM). Run 'just build' first." >&2
    exit 1
fi
if [ ! -f "$COMPILER_LGO" ] || [ "$COMPILER_ASM" -nt "$COMPILER_LGO" ]; then
    echo "Error: compiler .lgo missing or stale ($COMPILER_LGO). Run 'just build-lgo' first." >&2
    exit 1
fi

# Separate .msw and .plsw files
MACROS=()
MAIN=""
for f in "$@"; do
    case "$f" in
        *.msw) MACROS+=("$f") ;;
        *.plsw) MAIN="$f" ;;
        *) echo "Error: unknown file type: $f" >&2; exit 1 ;;
    esac
done

if [ -z "$MAIN" ]; then
    echo "Error: no .plsw file specified" >&2
    exit 1
fi

# Build UART input string with \n escapes for cor24-emu -u
# Uses FILE:/SOURCE: protocol when .msw files present, legacy otherwise.
build_input() {
    printf 'c\\n'
    if [ ${#MACROS[@]} -gt 0 ]; then
        # FILE: protocol -- each .msw becomes a named include
        for m in "${MACROS[@]}"; do
            printf 'FILE:%s\\n' "$(basename "$m")"
            while IFS= read -r line; do
                printf '%s\\n' "$line"
            done < "$m"
            printf '\\x1E'  # record separator = end of file content
        done
        printf 'SOURCE:\\n'
    fi
    while IFS= read -r line; do
        printf '%s\\n' "$line"
    done < "$MAIN"
    printf '\\x04'
}

INPUT=$(build_input)

# Compile: feed source to the PL/SW compiler running on the emulator
COMPILER_OUT=$(cor24-emu --lgo "$COMPILER_LGO" -u "$INPUT" -n 200000000 -t 120 --speed 0 2>&1)

# Extract the UART output block (multiline: from "UART output:" to "Executed")
UART_OUT=$(echo "$COMPILER_OUT" | sed -n '/^UART output:/,/^Executed /{/^Executed /d;p;}' | sed '1s/^UART output: //')

if echo "$UART_OUT" | grep -q "compilation failed\|COMPILE ERROR\|ERROR:"; then
    echo "Compilation failed:" >&2
    echo "$UART_OUT" | grep -E "ERROR:|failed" >&2
    exit 1
fi

# Extract assembly between markers
START_MARKER="--- generated assembly ---"
END_MARKER="--- end assembly ---"
ASM=$(echo "$UART_OUT" | sed -n "/$START_MARKER/,/$END_MARKER/{/$START_MARKER/d;/$END_MARKER/d;p;}")

if [ -z "$ASM" ]; then
    echo "Error: no assembly output found in compiler output" >&2
    echo "Compiler output (last 20 lines):" >&2
    echo "$UART_OUT" | tail -20 >&2
    exit 1
fi

# Per-invocation scratch directory. mktemp -d is the only call
# that needs the random suffix; files inside use deterministic
# names so we don't depend on BSD-vs-GNU mktemp differences with
# trailing extensions (`mktemp /tmp/foo-XXXXXX.s` is portable
# only if you trust both implementations to substitute the X's
# correctly when followed by a literal suffix). The directory
# isolation also prevents two parallel pipelines from ever
# touching the same path even hypothetically.
SCRATCH=$(mktemp -d /tmp/plsw-XXXXXX)
trap "rm -rf $SCRATCH" EXIT
TMPASM="$SCRATCH/program.s"
TMPLGO="$SCRATCH/program.lgo"
echo "$ASM" > "$TMPASM"

ASM_LINES=$(echo "$ASM" | wc -l | tr -d ' ')
echo "=== Compiled $(basename "$MAIN") ($ASM_LINES lines of assembly) ===" >&2

# Show registered includes if any
echo "$UART_OUT" | grep "registered:" >&2 || true

# Assemble + run the generated assembly
cor24-asm "$TMPASM" -o "$TMPLGO"
echo "=== Running ===" >&2
RUN_OUT=$(cor24-emu --lgo "$TMPLGO" -n 10000000 -t 30 --speed 0 2>&1)

# Extract program output (multiline UART output block)
PROG_OUT=$(echo "$RUN_OUT" | sed -n '/^UART output:/,/^Executed /{/^Executed /d;p;}' | sed '1s/^UART output: //')

if [ -n "$PROG_OUT" ]; then
    echo "$PROG_OUT"
else
    echo "(no output)" >&2
fi
