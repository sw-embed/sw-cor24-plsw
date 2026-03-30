# sw-cor24-plsw -- Claude Instructions

## Project Overview

PL/SW (Programming Language for Software Wrighter) compiler for the
COR24 24-bit RISC ISA. A freestanding, PL/I-inspired systems language
with AST-aware compile-time macros, transpiling to COR24 assembler
source (.s). Written in C (via tc24r cross-compiler), targeting the
COR24 emulator.

See `docs/` for detailed documentation:
- `docs/prd.md` -- requirements
- `docs/architecture.md` -- system architecture
- `docs/design.md` -- design decisions
- `docs/plan.md` -- phased implementation plan
- `docs/usage.md` -- language reference and usage guide
- `docs/research.txt` -- PL/SW design research and rationale

## CRITICAL: AgentRail Session Protocol (MUST follow exactly)

### 1. START (do this FIRST, before anything else)
```bash
agentrail next
```
Read the output carefully. It contains your current step, prompt,
plan context, and any relevant skills/trajectories.

### 2. BEGIN (immediately after reading the next output)
```bash
agentrail begin
```

### 3. WORK (do what the step prompt says)
Do NOT ask "want me to proceed?". The step prompt IS your instruction.
Execute it directly.

### 4. COMMIT (after the work is done)
Commit your code changes with git. Use `/mw-cp` for the checkpoint
process (pre-commit checks, docs, detailed commit, push).

### 5. COMPLETE (LAST thing, after committing)
```bash
agentrail complete --summary "what you accomplished" \
  --reward 1 \
  --actions "tools and approach used"
```
- If the step failed: `--reward -1 --failure-mode "what went wrong"`
- If the saga is finished: add `--done`

### 6. STOP (after complete, DO NOT continue working)
Do NOT make further code changes after running `agentrail complete`.
Any changes after complete are untracked and invisible to the next
session. Future work belongs in the NEXT step, not this one.

## Key Rules

- **Do NOT skip steps** -- the next session depends on accurate tracking
- **Do NOT ask for permission** -- the step prompt is the instruction
- **Do NOT continue working** after `agentrail complete`
- **Commit before complete** -- always commit first, then record completion

## Useful Commands

```bash
agentrail status          # Current saga state
agentrail history         # All completed steps
agentrail plan            # View the plan
agentrail next            # Current step + context
```

## Build / Test

This project uses tc24r (C compiler) and cor24-run (emulator):

```bash
# Compile C to COR24 assembly
tc24r src/plsw.c -o build/plsw.s

# Assemble and run on emulator
cor24-run --run build/plsw.s
```

Stable toolchain references:
- `~/github/sw-vibe-coding/tc24r` -- C compiler source
- `~/github/sw-embed/sw-cor24-emulator` -- emulator + assembler
- `~/github/sw-embed/sw-cor24-tinyc` -- tc24r compiler (Rust)

Sibling language projects for reference:
- `~/github/sw-embed/sw-cor24-apl` -- APL interpreter (C via tc24r)
- `~/github/sw-embed/sw-cor24-pascal` -- Pascal compiler (C via tc24r)
- `~/github/sw-vibe-coding/tml24c` -- Macro Lisp compiler (C via tc24r)

## COR24 Quick Reference

- 24-bit RISC, 8 registers (r0-r2 GP, fp, sp, z, iv, ir)
- 1 MB SRAM + 8 KB EBR stack
- UART at 0xFF0100 (data), 0xFF0101 (status)
- LED at 0xFF0000
- No hardware divide -- use software division
- Variable-length instructions (1, 2, or 4 bytes)
- Calling convention: args on stack (R-to-L), return in r0, link via stack
- Prologue: push fp, push r2, push r1, mov fp,sp
- First arg at fp+9 (3 saved regs x 3 bytes)

## PL/SW Language Summary

- Source files: `.plsw`, macro files: `.msw`
- PL/I-like syntax: DCL, PROC, DO/END, IF/THEN/ELSE
- Integer-only types: BIT, BYTE, WORD, INT(n), CHAR, PTR
- Level-based structure declarations (1, 3, 5, ...)
- Macro invocation: `?MACRO(KEYWORD(value), ...)`
- Inline assembly: `ASM DO; "instr"; END;`
- No floats, no runtime, freestanding
