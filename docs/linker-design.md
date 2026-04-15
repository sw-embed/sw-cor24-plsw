# link24 -- Linker Design

## Problem

A large PL/SW program (e.g., the SNOBOL4 interpreter) needs to be
split into separately compiled modules. The assembler (as24/cor24-run)
cannot be modified. The linker must produce a single combined binary
from independently assembled modules.

## Solution: FIXUP-Based Linking

Each module compiles to a `.s` file and assembles to a `.bin` file.
A `.meta` file describes exported symbols and FIXUP locations where
external references need patching. The linker concatenates the
binaries and patches all external references.

No jump tables. No `.s` concatenation. Every external `la rN,<sym>`
instruction gets patched directly with the resolved address.

## Architecture

```
 .plsw source files (one per module)
    |
    v
 PL/SW compiler (one invocation per module)
    - Main module: normal compilation
    - Library modules: source includes %DEFINE LIBRARY;
      which suppresses runtime preamble, MAIN wrapper,
      and shared globals (DCL names starting with _)
    |
    +---> module.s       (assembly, library-clean)
    |
    v
 meta-gen prep (rewrite external refs to la rN,0)
    |
    +---> module.s       (assembleable, placeholders for externals)
    +---> module.syms    (external reference list)
    |
    v
 cor24-run --assemble (pass 1, base 0)
    |
    +---> module.bin     (for size measurement)
    +---> module.lst     (listing with byte offsets)
    |
    v
 meta-gen emit (parse .lst + .syms)
    |
    +---> module.meta    (EXPORT + FIXUP entries)
    |
    v
 [compute layout: assign base addresses from sizes]
    |
    v
 cor24-run --assemble (pass 2, with --base-addr)
    |
    +---> module.bin     (final binary, internal refs resolved)
    |
    v
 link24 (all .bin + .meta files)
    |
    +---> program.bin    (combined binary, all FIXUPs applied)
    +---> program.map    (symbol table for debugging)
```

## Tools

### link24

Combines `.bin` files and applies FIXUP patches.

```bash
link24 --entry <module> [--dir <path>] [-o <out.bin>] [--map <out.map>] <modules...>
```

The entry module is placed at address 0. Remaining modules follow
in the order specified.

### meta-gen

Two-phase tool for generating `.meta` files from `.s`/`.lst` files.

```bash
# Phase 1: rewrite external refs to placeholders
meta-gen prep <input.s> -o <output.s> [--syms <output.syms>]

# Phase 2: generate .meta from assembled listing
meta-gen emit <input.lst> --syms <input.syms> --module <name> -o <output.meta>
```

Both tools are in `components/linker/` (Rust, built with cargo).

## .meta File Format

Plain text, semicolon comments.

```
MODULE sno_util

EXPORT _SLEN       0x0000
EXPORT _SCEQ       0x002A
EXPORT _IN_CLASS   0x0048

FIXUP 0x0012 _UART_PUTS
FIXUP 0x003E _UART_PUTS
FIXUP 0x006C _COUNT
```

### Directives

- `MODULE <name>` -- module identifier (must match filename stem)
- `EXPORT <symbol> <hex_offset>` -- symbol defined in this module,
  at the given byte offset from module start
- `FIXUP <hex_offset> <symbol>` -- at this offset there is an `la`
  instruction whose 3 address bytes (little-endian, at offset+1..+3)
  must be patched with the resolved address of `<symbol>`

## How FIXUP Patching Works

COR24 `la` instructions are 4 bytes:

```
  Byte 0:    opcode (0x29=r0, 0x2A=r1, 0x2B=r2)
  Bytes 1-3: 24-bit address (little-endian)
```

When meta-gen prep rewrites `la r2,_UART_PUTS` to `la r2,0`, the
assembled binary has `[0x2B] [0x00] [0x00] [0x00]` at that offset.

The linker resolves `_UART_PUTS` to its absolute address (e.g.,
0x000024) and patches bytes 1-3: `[0x2B] [0x24] [0x00] [0x00]`.

## Two-Pass Assembly

The assembler resolves all label references at assembly time, so
each module must be assembled with `--base-addr` set to its final
base address. This requires two passes:

1. **Pass 1** (base 0): Assemble to get module sizes and `.lst`
   files for meta-gen.
2. **Layout**: Compute base addresses from sizes.
3. **Pass 2** (with `--base-addr`): Reassemble so internal label
   references (strings, local functions) resolve to correct absolute
   addresses.

The `.meta` file uses pass-1 offsets (module-relative). The linker
adds each module's base address when applying FIXUPs.

## Build Flow (Step by Step)

This is the exact sequence for building a multi-module program.
The demo script `tests/demo-fixup.sh` implements this.

### Prerequisites

```bash
cd components/linker && cargo build --release
# Produces: target/release/link24, target/release/meta-gen
```

### Step 1: Compile each module with PL/SW

```bash
# Main module: normal compilation (emits runtime + shared globals)
plsw-compile src/sno_main.plsw -o build/sno_main.s

# Library modules: source begins with %DEFINE LIBRARY;
# This suppresses: _start, _halt, _UART_PUTCHAR, _UART_PUTS,
# MAIN wrapper, and all DCL names starting with _ (shared globals).
plsw-compile src/sno_util.plsw -o build/sno_util.s
plsw-compile src/sno_lex.plsw  -o build/sno_lex.s
plsw-compile src/sno_exec.plsw -o build/sno_exec.s
```

No strip-runtime.sh needed. The `%DEFINE LIBRARY` directive handles
everything at compile time.

### Step 2: meta-gen prep (rewrite external refs)

```bash
for mod in sno_main sno_util sno_lex sno_exec; do
    meta-gen prep build/${mod}.s \
        -o build/${mod}_prep.s \
        --syms build/${mod}.syms
done
```

This scans each `.s` file, identifies `la rN,_SYMBOL` where
`_SYMBOL` is not defined locally, replaces with `la rN,0`, and
records the external reference in the `.syms` file.

### Step 3: Pass 1 assembly (base 0)

```bash
for mod in sno_main sno_util sno_lex sno_exec; do
    cor24-run --assemble build/${mod}_prep.s \
        build/${mod}.bin build/${mod}.lst
done
```

### Step 4: meta-gen emit (generate .meta)

```bash
for mod in sno_main sno_util sno_lex sno_exec; do
    meta-gen emit build/${mod}.lst \
        --syms build/${mod}.syms \
        --module $mod \
        -o build/${mod}.meta
done
```

### Step 5: Compute layout and reassemble

```bash
# Get sizes (in bytes)
MAIN_SIZE=$(stat -f%z build/sno_main.bin)     # or wc -c
UTIL_SIZE=$(stat -f%z build/sno_util.bin)
LEX_SIZE=$(stat -f%z build/sno_lex.bin)

# Compute bases
MAIN_BASE=0
UTIL_BASE=$((MAIN_SIZE))
LEX_BASE=$((MAIN_SIZE + UTIL_SIZE))
EXEC_BASE=$((MAIN_SIZE + UTIL_SIZE + LEX_SIZE))

# Pass 2: reassemble with correct base addresses
cor24-run --assemble build/sno_main_prep.s build/sno_main.bin build/sno_main.lst --base-addr $MAIN_BASE
cor24-run --assemble build/sno_util_prep.s build/sno_util.bin build/sno_util.lst --base-addr $UTIL_BASE
cor24-run --assemble build/sno_lex_prep.s  build/sno_lex.bin  build/sno_lex.lst  --base-addr $LEX_BASE
cor24-run --assemble build/sno_exec_prep.s build/sno_exec.bin build/sno_exec.lst --base-addr $EXEC_BASE
```

### Step 6: Link

```bash
link24 --entry sno_main --dir build --map build/program.map \
    sno_main sno_util sno_lex sno_exec \
    -o build/program.bin
```

### Step 7: Run

```bash
cor24-run --load-binary build/program.bin@0 --entry 0 --terminal
```

## Shared Globals and the LIBRARY Mode Rule

**In LIBRARY mode, the compiler suppresses ALL top-level DCL data
emission.** This is a strict rule:

- **Main module** (no `%DEFINE LIBRARY`): emits all top-level DCLs
  (BASED records, plain globals, arrays) in its `.data` section.
  These are EXPORTed in its `.meta`.

- **Library modules** (`%DEFINE LIBRARY`): emit NO top-level DCL
  data. References to those symbols in code become unresolved,
  meta-gen rewrites them to `la rN,0` placeholders, and link24
  patches them with addresses from the main module via FIXUP.

### Implications for Library Module Source

Library modules **must not** have any top-level DCLs that they own.
Specifically:

1. **Shared globals from `.msw` includes** (e.g., `snoglob.msw`,
   `descr.msw`, `heap.msw`): These are owned by the main module.
   Library modules can include the same `.msw` files for type
   checking and to get the symbol names in scope -- the data
   emission is suppressed automatically.

2. **Module-local persistent state**: Must be declared inside
   procedures as `STATIC` variables, not at top level. Example:
   ```
   /* WRONG in library module: */
   DCL N_OUT(8) CHAR INIT('OUTPUT');

   /* RIGHT in library module: */
   PROC GET_OUTPUT_NAME RETURNS(PTR);
       DCL N_OUT(8) CHAR STATIC INIT('OUTPUT');
       RETURN ADDR(N_OUT);
   END;
   ```

3. **BASED record descriptors**: These are type templates owned
   by the main module. Library modules include the `.msw` file
   for type info; data is suppressed.

If link24 reports `duplicate export '_FOO'`, it means a library
module has a top-level DCL that wasn't suppressed -- check that
`%DEFINE LIBRARY;` appears before any DCL in the library source.

If link24 reports `unresolved symbol '_FOO'`, it means the main
module doesn't define `_FOO` -- add the DCL to the main module
or move the DCL there from the library module.

## Error Handling

| Condition | Action |
|---|---|
| Unresolved FIXUP symbol | Error with symbol name, module, and offset |
| Duplicate EXPORT | Error naming both modules |
| Missing .meta or .bin file | Error with filename |
| Entry module not found | Error |
| FIXUP offset out of range | Error (meta-gen or assembly bug) |
| Combined binary exceeds 1 MB | Warning |

## Test / Demo

### Files

```
components/linker/
  src/
    main.rs             link24 linker
    meta_gen.rs         meta-gen tool
  tests/
    demo-fixup.sh       end-to-end test driver (bash)
    fixtures-fixup/
      main.s            entry module (runtime + globals + _ENTRY)
      liba.s            library A (calls _UART_PUTS, _UTIL_INC)
      libb.s            library B (calls _UART_PUTS, _UTIL_INC)
      util.s            utility (calls _UART_PUTS, refs _COUNT)
```

### Module Call Graph

```
  main -----> liba -----> util (refs _COUNT, calls _UART_PUTS)
    |                       ^
    +-------> libb ---------+
```

- **main**: Defines _start, _UART_PUTCHAR, _UART_PUTS, _ENTRY,
  and shared globals _MODE, _COUNT. Calls _LIB_A and _LIB_B.
- **liba**: Calls _UART_PUTS (from main) and _UTIL_INC (from util).
- **libb**: Same pattern as liba.
- **util**: Calls _UART_PUTS (from main), reads/writes _COUNT
  (shared global from main).

### Expected Output

```
main:enter
liba:enter
util:inc
liba:leave
libb:enter
util:inc
libb:leave
main:leave
```

### Running the Demo

```bash
cd components/linker
cargo build --release
/bin/bash tests/demo-fixup.sh
```

### reg-rs Regression Test

```bash
reg-rs run -p link24_fixup_cross_module
```

## Implementation Notes

### link24 Source: `components/linker/src/main.rs`

- Parses `.meta` files (MODULE, EXPORT, FIXUP)
- Assigns base addresses sequentially (entry module first)
- Builds global symbol table from all EXPORTs
- Concatenates `.bin` files into output buffer
- Applies FIXUPs: patches bytes offset+1..+3 of each `la`
  instruction with the resolved 24-bit address (little-endian)

### meta-gen Source: `components/linker/src/meta_gen.rs`

- `prep` phase: scans `.s` for label definitions, identifies
  external `la rN,_SYMBOL` references, rewrites to `la rN,0`,
  emits `.syms` file
- `emit` phase: parses `.lst` for byte offsets of labels and
  `la rN,0` instructions, cross-references with `.syms` to
  produce `.meta`

### COR24 `la` Instruction Encoding

```
  Opcode bytes: r0=0x29, r1=0x2A, r2=0x2B
  Total size: 4 bytes (1 opcode + 3 address, little-endian)
  FIXUP patches bytes 1-3 with resolved address
```

Note: `la r7,<addr>` (opcode 0xC7) is an absolute jump (sets PC).
r7 is the instruction register (ir), not a GPR. The assembler does
not accept `la r7,<addr>` syntax; it must be emitted as raw bytes.
This encoding is not used by the FIXUP mechanism.

## Constraints

- All modules must fit in 1 MB SRAM total.
- FIXUP only patches `la` instructions (4-byte, address in bytes 1-3).
- External symbols must start with `_` and be uppercase.
- Module names must match between filenames and MODULE directives.
- Shared globals must be defined in exactly one module (the main).
