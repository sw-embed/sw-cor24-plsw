#!/bin/bash
# demo-plsw-modular.sh -- End-to-end PL/SW multi-module link test
#
# Drives the full flow from .plsw source through link24:
#   1. Compile each .plsw module via the PL/SW compiler
#      (cor24-emu --lgo build/plsw.lgo)
#   2. meta-gen prep: rewrite external la refs to placeholders
#   3. Pass-1 assemble (base 0) via cor24-asm to obtain sizes + .lst
#   4. meta-gen emit: produce .meta with EXPORT/FIXUP from pass-1 .lst
#   5. Compute layout
#   6. Pass-2 assemble with --base-addr (internal refs resolved at final addrs)
#   7. link24 to combined binary
#   8. cor24-emu --load-binary on emulator
#
# Modules:
#   main  -- entry module: runtime + MAIN, calls LIB_HI
#   lib   -- %DEFINE LIBRARY: LIB_HI calls UART_PUTCHAR (external -> FIXUP)
#
# Expected output:  HI
#
# Toolchain notes (post bootstrap-toolchain saga):
#   * Compile uses cor24-emu --lgo on a pre-built build/plsw.lgo
#     (auto-built below from build/plsw.s if missing).
#   * Pass-1 assembly uses cor24-asm.
#   * Final execution uses cor24-emu --load-binary --entry (same
#     flags as the old cor24-run).
#   * Pass-2 still uses cor24-run --assemble --base-addr because
#     cor24-asm 0.1.0 has no --base-addr equivalent yet. dcxas has
#     a follow-up brief to expose the library-level base_address
#     via the CLI; once that lands, a small follow-up PR flips the
#     remaining two lines and cor24-run can be retired here.
#
# Requires: components/linker built (cargo build --release in
# components/linker), the PL/SW compiler at build/plsw.s
# (just build) plus build/plsw.lgo (just build-lgo), cor24-asm and
# cor24-emu on PATH (cor24-run still required for pass-2 only).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
FIXTURES="$SCRIPT_DIR/fixtures-plsw"
PLSW_ASM="${PLSW_ASM:-$REPO_ROOT/build/plsw.s}"
PLSW_LGO="${PLSW_LGO:-$REPO_ROOT/build/plsw.lgo}"

BIN_DIR="${SCRIPT_DIR}/../target/release"
if [ ! -f "$BIN_DIR/link24" ]; then
    BIN_DIR="${SCRIPT_DIR}/../target/debug"
fi
LINK24="$BIN_DIR/link24"
META_GEN="$BIN_DIR/meta-gen"

if [ ! -f "$PLSW_ASM" ]; then
    echo "Error: PL/SW compiler not built ($PLSW_ASM). Run 'just build'." >&2
    exit 1
fi
if [ ! -f "$PLSW_LGO" ] || [ "$PLSW_ASM" -nt "$PLSW_LGO" ]; then
    cor24-asm "$PLSW_ASM" -o "$PLSW_LGO"
fi
if [ ! -x "$LINK24" ] || [ ! -x "$META_GEN" ]; then
    echo "Error: linker tools not built. Run 'cargo build --release' in components/linker." >&2
    exit 1
fi

TMPDIR=$(mktemp -d /tmp/plsw-modular-XXXXXX)
trap "rm -rf $TMPDIR" EXIT

MODULES=(main lib)

# --- Phase 0: compile each .plsw to .s via the PL/SW compiler on emulator ---
compile_plsw() {
    local src="$1"
    local out="$2"
    # Build UART input: 'c\n' then source then \x04 (EOT)
    local input
    input=$(printf 'c\\n'; while IFS= read -r line; do printf '%s\\n' "$line"; done < "$src"; printf '\\x04')
    local raw
    raw=$(cor24-emu --lgo "$PLSW_LGO" -u "$input" -n 200000000 -t 120 --speed 0 2>&1)
    local uart
    uart=$(echo "$raw" | sed -n '/^UART output:/,/^Executed /{/^Executed /d;p;}' | sed '1s/^UART output: //')
    if echo "$uart" | grep -q "ERROR:\|compilation failed\|COMPILE ERROR"; then
        echo "Compile error in $src:" >&2
        echo "$uart" | grep -E "ERROR:|failed" >&2
        exit 1
    fi
    echo "$uart" | sed -n '/--- generated assembly ---/,/--- end assembly ---/{/--- /d;p;}' > "$out"
    if [ ! -s "$out" ]; then
        echo "Error: no assembly produced from $src" >&2
        echo "$uart" | tail -20 >&2
        exit 1
    fi
}

for mod in "${MODULES[@]}"; do
    echo "=== Compiling $mod.plsw ===" >&2
    compile_plsw "$FIXTURES/${mod}.plsw" "$TMPDIR/${mod}.s.raw"
done

# --- Phase 1: meta-gen prep (rewrite external la refs to placeholders) ---
for mod in "${MODULES[@]}"; do
    "$META_GEN" prep "$TMPDIR/${mod}.s.raw" \
        -o "$TMPDIR/${mod}.s" \
        --syms "$TMPDIR/${mod}.syms"
done

# --- Phase 2: Pass 1 assemble at base 0 (cor24-asm) ---
SIZES=()
for mod in "${MODULES[@]}"; do
    cor24-asm "$TMPDIR/${mod}.s" \
        --bin "$TMPDIR/${mod}.bin" --listing "$TMPDIR/${mod}.lst" >/dev/null
    SIZES+=($(wc -c < "$TMPDIR/${mod}.bin"))
done

# --- Phase 3: meta-gen emit (.meta from pass-1 .lst) ---
for mod in "${MODULES[@]}"; do
    "$META_GEN" emit "$TMPDIR/${mod}.lst" \
        --syms "$TMPDIR/${mod}.syms" \
        --module "$mod" \
        -o "$TMPDIR/${mod}.meta"
done

# --- Phase 4: layout ---
BASES=()
ADDR=0
for i in "${!MODULES[@]}"; do
    BASES+=($ADDR)
    ADDR=$((ADDR + ${SIZES[$i]}))
done

# --- Phase 5: Pass 2 assemble with --base-addr ---
for i in "${!MODULES[@]}"; do
    cor24-run --assemble "$TMPDIR/${MODULES[$i]}.s" \
        "$TMPDIR/${MODULES[$i]}.bin" "$TMPDIR/${MODULES[$i]}.lst" \
        --base-addr "${BASES[$i]}" >/dev/null
done

# --- Phase 6: link24 ---
"$LINK24" --entry main --dir "$TMPDIR" --map "$TMPDIR/program.map" \
    main lib -o "$TMPDIR/program.bin" >/dev/null

# --- Phase 7: run on emulator ---
OUTPUT=$(cor24-emu --load-binary "$TMPDIR/program.bin@0" \
    --entry 0 --speed 0 -n 10000000 -t 10 2>&1 \
    | awk '/^UART output:/{sub(/^UART output: /,"");f=1} f{if(/^$/)exit;print}')

EXPECTED="HI"
if [ "$OUTPUT" = "$EXPECTED" ]; then
    echo "PASS: got '$OUTPUT'"
    exit 0
else
    echo "FAIL: expected '$EXPECTED', got '$OUTPUT'" >&2
    exit 1
fi
