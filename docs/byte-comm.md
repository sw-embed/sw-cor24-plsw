# .byte and .comm Assembler Directives — Compatibility Report

## Context

The PL/SW compiler generates COR24 assembly (.s files) that are assembled
by the cor24-run Rust assembler. The reference assembler is as24 (C),
available as a REST service on queenbee:7412.

This document describes the directives used by the PL/SW compiler and
where the cor24-run assembler's behavior should match as24.

## Directives Used

### `.byte <value>`
Emit a single byte.

**cor24-run**: Works. `value` is a decimal integer or label.
**as24**: Works. Also accepts hex with `h` suffix (e.g., `0FFh`).

**Issue**: cor24-run accepts `0xFF` hex prefix syntax. as24 uses `0FFh`
suffix syntax. The PL/SW compiler emits decimal only, so this is not
a problem in practice.

### `.word <value>`
Emit a 24-bit (3-byte) word.

**cor24-run**: Works.
**as24**: Works.

### `.comm <symbol>, <size>`
Reserve `size` bytes of zero-initialized (BSS) space. Defines `symbol`
as a label at the start of the reserved region.

**cor24-run**: Supported (assembler.rs:289). Advances the address counter
by `size` without emitting bytes. The symbol is registered in the label
table at the current address.

**as24**: Supported. Same semantics — reserves space in the BSS section.

**Behavior verified**:
```
.data
.comm _ARENA, 512
_AFTER:
.byte 0xFF
```

Both assemblers place `_ARENA` at offset 0, `_AFTER` at offset 512.
The 512 bytes are not in the binary output (zero-initialized in SRAM).

**Use case**: The PL/SW compiler uses `.comm` for large uninitialized
arrays (arena storage, record templates) to avoid emitting hundreds
of `.byte 0` lines. Reduces chain.plsw assembly from 1495 to 447 lines.

### `.data`, `.text`
Section switching directives.

**cor24-run**: Accepted but ignored (flat memory model).
**as24**: Tracks sections for the linker (ld24).

**Note**: In the cor24-run flat model, `.data` and `.text` content is
interleaved in address order. The PL/SW compiler emits `.text` first
(procedures), then `.data` (static variables and string literals).

### `.globl <symbol>`
Declare a symbol as global (visible to linker).

**cor24-run**: Accepted but ignored (no linker).
**as24**: Marks symbol for export.

## Directives NOT Supported by cor24-run

### `.bss`
Switch to BSS section.

**as24**: Supported. Subsequent labels and `.comm` go in BSS.
**cor24-run**: Not recognized (silently ignored or unknown).

**Impact**: Not needed — `.comm` in the `.data` section achieves
the same effect for the PL/SW use case.

### `.space <size>` / `.zero <size>`
Reserve `size` bytes of zeros.

**as24**: Not tested.
**cor24-run**: Silently ignored — emits 0 bytes.

**Impact**: Use `.comm symbol, size` instead.

## Instruction Notes

### `sb z,N(r2)` — Store zero register with offset
**cor24-run**: Not supported. `z` cannot be used as a source register
for `sb` with an offset operand.

**Workaround**: Use `lc r0,0` then `sb r0,N(r2)`.

## Recommendations for cor24-run

1. **`.comm` is working** — no changes needed for current PL/SW use cases.

2. **Consider adding `.space N`** as an alias for `.comm` without a label,
   for inline zero-padding after partial initializers (e.g., `DCL BUF(80)
   CHAR INIT('Hi')` — 2 bytes of data + 78 bytes of zeros).

3. **Consider supporting `sb z,N(r2)`** — the zero register (`z`) is
   useful for clearing fields without loading 0 into a GP register first.

4. **Listing output**: The cor24-run `.lst` file does not carry through
   assembly-level comments (`;` lines). Source line comments from the
   PL/SW compiler (e.g., `; 98:     CVTPTR = ALLOC(14);`) are lost in
   the listing. Consider preserving them.
