#!/bin/bash
# pipeline-dump.sh -- Compile .plsw and run with memory dump
#
# Usage: ./scripts/pipeline-dump.sh [macro.msw ...] program.plsw
#
# Outputs:
#   build/<basename>.s        -- generated assembly
#   build/<basename>.lgo      -- assembled program (cor24-asm output)
#   build/<basename>-dump.txt -- full emulator dump (registers, SRAM, stack, I/O)
#   stdout                    -- UART output from program
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

if [ ! -f "$COMPILER_ASM" ]; then
    echo "Error: compiler not built ($COMPILER_ASM). Run 'just build' first." >&2
    exit 1
fi
if [ ! -f "$COMPILER_LGO" ] || [ "$COMPILER_ASM" -nt "$COMPILER_LGO" ]; then
    cor24-asm "$COMPILER_ASM" -o "$COMPILER_LGO"
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

# Build UART input
build_input() {
    printf 'c\\n'
    if [ ${#MACROS[@]} -gt 0 ]; then
        for m in "${MACROS[@]}"; do
            printf 'FILE:%s\\n' "$(basename "$m")"
            while IFS= read -r line; do
                printf '%s\\n' "$line"
            done < "$m"
            printf '\\x1E'
        done
        printf 'SOURCE:\\n'
    fi
    while IFS= read -r line; do
        printf '%s\\n' "$line"
    done < "$MAIN"
    printf '\\x04'
}

INPUT=$(build_input)

# Save combined source with file-path comments
BASENAME=$(basename "$MAIN" .plsw)
OUT_COMBINED="build/${BASENAME}-combined.plsw"
{
    for m in "${MACROS[@]+"${MACROS[@]}"}"; do
        echo "/* ---- $(realpath "$m") ---- */"
        cat "$m"
        echo ""
    done
    echo "/* ---- $(realpath "$MAIN") ---- */"
    cat "$MAIN"
} > "$OUT_COMBINED"
echo "Combined source: $OUT_COMBINED" >&2

# Compile
echo "=== Compiling $(basename "$MAIN") ===" >&2
COMPILER_OUT=$(cor24-emu --lgo "$COMPILER_LGO" -u "$INPUT" -n 200000000 -t 120 --speed 0 2>&1)
UART_OUT=$(echo "$COMPILER_OUT" | sed -n '/^UART output:/,/^Executed /{/^Executed /d;p;}' | sed '1s/^UART output: //')

if echo "$UART_OUT" | grep -q "compilation failed\|COMPILE ERROR\|ERROR:"; then
    echo "Compilation failed:" >&2
    echo "$UART_OUT" | grep -E "ERROR:|failed" >&2
    exit 1
fi

# Extract assembly
START_MARKER="--- generated assembly ---"
END_MARKER="--- end assembly ---"
ASM=$(echo "$UART_OUT" | sed -n "/$START_MARKER/,/$END_MARKER/{/$START_MARKER/d;/$END_MARKER/d;p;}")

if [ -z "$ASM" ]; then
    echo "Error: no assembly output" >&2
    exit 1
fi

# Derive output names from input .plsw filename
BASENAME=$(basename "$MAIN" .plsw)
OUT_S="build/${BASENAME}.s"
OUT_LGO="build/${BASENAME}.lgo"
OUT_DUMP="build/${BASENAME}-dump.txt"

# Save assembly
echo "$ASM" > "$OUT_S"
ASM_LINES=$(echo "$ASM" | wc -l | tr -d ' ')
echo "Assembly: $ASM_LINES lines -> $OUT_S" >&2

# Show registered includes
echo "$UART_OUT" | grep "registered:" >&2 || true

# Assemble + run with --dump
cor24-asm "$OUT_S" -o "$OUT_LGO"
echo "=== Running with --dump ===" >&2
RUN_OUT=$(cor24-emu --lgo "$OUT_LGO" -n 50000000 -t 30 --speed 0 --dump 2>&1)

# Save full dump
echo "$RUN_OUT" > "$OUT_DUMP"
echo "Dump saved to $OUT_DUMP" >&2

# Show UART output
PROG_OUT=$(echo "$RUN_OUT" | sed -n '/^UART output:/,/^Executed /{/^Executed /d;p;}' | sed '1s/^UART output: //')
if [ -n "$PROG_OUT" ]; then
    echo "$PROG_OUT"
fi

# Show key stats
echo "$RUN_OUT" | grep -E "^  Instructions:|^  Halted:" >&2
