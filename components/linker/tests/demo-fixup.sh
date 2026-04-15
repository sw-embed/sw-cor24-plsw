#!/bin/bash
# demo-fixup.sh -- End-to-end demo of FIXUP-based separate compilation
#
# 4 modules: main (entry + globals + runtime), liba, libb, util
# Cross-module function calls and shared global data references
# are resolved via FIXUP patching -- no jump tables.
#
# Build flow:
#   1. meta-gen prep: rewrite external la refs to la rN,0 placeholders
#   2. Pass 1 assembly: assemble at base 0 to get sizes
#   3. Compute layout (base addresses)
#   4. Pass 2 assembly: reassemble with --base-addr
#   5. meta-gen emit: parse .lst to produce .meta with EXPORT/FIXUP
#   6. link24: produce combined binary
#   7. cor24-run: execute
#
# Expected output:
#   main:enter
#   liba:enter
#   util:inc
#   liba:leave
#   libb:enter
#   util:inc
#   libb:leave
#   main:leave

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURES="$SCRIPT_DIR/fixtures-fixup"
BIN_DIR="${SCRIPT_DIR}/../target/release"
if [ ! -f "$BIN_DIR/link24" ]; then
    BIN_DIR="${SCRIPT_DIR}/../target/debug"
fi
LINK24="$BIN_DIR/link24"
META_GEN="$BIN_DIR/meta-gen"

TMPDIR=$(mktemp -d /tmp/link24-fixup-XXXXXX)
trap "rm -rf $TMPDIR" EXIT

# Module order: entry module first
MODULES=(main liba libb util)

# --- Phase 1: meta-gen prep (rewrite external refs) ---
# Main module has no external refs (it defines everything).
# Library modules reference _UART_PUTS, _UTIL_INC, _COUNT, etc.
for mod in "${MODULES[@]}"; do
    "$META_GEN" prep "$FIXTURES/${mod}.s" \
        -o "$TMPDIR/${mod}.s" \
        --syms "$TMPDIR/${mod}.syms"
done

# --- Phase 2: Pass 1 assembly (base 0) -> sizes + .lst for meta-gen ---
SIZES=()
for mod in "${MODULES[@]}"; do
    cor24-run --assemble "$TMPDIR/${mod}.s" \
        "$TMPDIR/${mod}.bin" "$TMPDIR/${mod}.lst" >/dev/null
    SIZES+=($(stat -f%z "$TMPDIR/${mod}.bin"))
done

# --- Phase 3: meta-gen emit (produce .meta from pass-1 .lst) ---
# Must use pass-1 .lst (base 0) so offsets are module-relative.
for mod in "${MODULES[@]}"; do
    "$META_GEN" emit "$TMPDIR/${mod}.lst" \
        --syms "$TMPDIR/${mod}.syms" \
        --module "$mod" \
        -o "$TMPDIR/${mod}.meta"
done

# --- Phase 4: Compute base addresses ---
BASES=()
ADDR=0
for i in "${!MODULES[@]}"; do
    BASES+=($ADDR)
    ADDR=$((ADDR + ${SIZES[$i]}))
done

# --- Phase 5: Pass 2 assembly (with --base-addr for internal refs) ---
for i in "${!MODULES[@]}"; do
    cor24-run --assemble "$TMPDIR/${MODULES[$i]}.s" \
        "$TMPDIR/${MODULES[$i]}.bin" "$TMPDIR/${MODULES[$i]}.lst" \
        --base-addr "${BASES[$i]}" >/dev/null
done

# --- Phase 6: link24 ---
"$LINK24" --entry main --dir "$TMPDIR" --map "$TMPDIR/program.map" \
    main liba libb util -o "$TMPDIR/program.bin" >/dev/null

# --- Phase 7: Run (entry module is at address 0, _start is at +0) ---
cor24-run --load-binary "$TMPDIR/program.bin@0" \
    --entry 0 --speed 0 -n 10000000 -t 10 \
    | awk '/^UART output:/{sub(/^UART output: /,"");f=1} f{if(/^$/)exit;print}'
