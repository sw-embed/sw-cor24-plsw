#!/bin/bash
# pipeline.sh -- End-to-end PL/SW compilation and execution pipeline
#
# Usage: ./scripts/pipeline.sh [macro.msw ...] program.plsw
#
# Feeds 'c' mode to the PL/SW compiler, sends macro and source files,
# extracts generated COR24 assembly, and runs the result.
#
# Requires: cor24-run, the compiler assembly at build/plsw.s

set -euo pipefail

COMPILER_ASM="${COMPILER_ASM:-build/plsw.s}"

if [ $# -lt 1 ]; then
    echo "Usage: $0 [macro.msw ...] program.plsw" >&2
    exit 1
fi

# Verify compiler exists
if [ ! -f "$COMPILER_ASM" ]; then
    echo "Error: compiler not built. Run 'just build' first." >&2
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

# Build UART input string with \n escapes for cor24-run -u
# Format: "c\n" + macro contents + main source + "\x04" (EOT)
build_input() {
    printf 'c\\n'
    for m in "${MACROS[@]+"${MACROS[@]}"}"; do
        printf '/* --- %s --- */\\n' "$(basename "$m")"
        while IFS= read -r line; do
            printf '%s\\n' "$line"
        done < "$m"
    done
    while IFS= read -r line; do
        printf '%s\\n' "$line"
    done < "$MAIN"
    printf '\\x04'
}

INPUT=$(build_input)

# Compile: run source through the PL/SW compiler on the emulator
COMPILER_OUT=$(cor24-run --run "$COMPILER_ASM" -u "$INPUT" -n 200000000 -t 120 --speed 0 2>&1)

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

# Write assembly to temp file
TMPASM=$(mktemp /tmp/plsw-XXXXXX.s)
trap "rm -f $TMPASM" EXIT
echo "$ASM" > "$TMPASM"

ASM_LINES=$(echo "$ASM" | wc -l | tr -d ' ')
echo "=== Compiled $(basename "$MAIN") ($ASM_LINES lines of assembly) ===" >&2

# Run the generated assembly
echo "=== Running ===" >&2
RUN_OUT=$(cor24-run --run "$TMPASM" -n 10000000 -t 30 --speed 0 2>&1)

# Extract program output (multiline UART output block)
PROG_OUT=$(echo "$RUN_OUT" | sed -n '/^UART output:/,/^Executed /{/^Executed /d;p;}' | sed '1s/^UART output: //')

if [ -n "$PROG_OUT" ]; then
    echo "$PROG_OUT"
else
    echo "(no output)" >&2
fi
