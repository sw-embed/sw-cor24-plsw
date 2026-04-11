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

**If `agentrail next` reports no current step (saga complete or
paused): STOP and ask the user what to do. Do NOT invent work.**
Don't go fix bugs, don't refactor, don't "just clean up" -- the
absence of a step is a signal that the next saga has not been
planned yet, and ad-hoc work between sagas creates orphan commits
that `agentrail audit` cannot reconcile.

### 2. BEGIN (immediately after reading the next output)
```bash
agentrail begin
```

### 3. WORK (do what the step prompt says)
Do NOT ask "want me to proceed?". The step prompt IS your instruction.
Execute it directly. Do NOT expand scope -- if you notice unrelated
problems, note them for a future step rather than silently fixing
them in this one.

### 4. COMMIT (after the work is done)
Commit your code changes with git. Use `/mw-cp` for the checkpoint
process (pre-commit checks, docs, detailed commit, push).

**Include any `.agentrail/` files you touched in the same commit
as your code changes** -- step.toml, summary.md, saga.toml, plan.md,
new step directories. They are part of the durable record and
`agentrail audit` relies on them being committed.

### 5. COMPLETE (LAST thing, after committing)
```bash
agentrail complete --summary "what you accomplished" \
  --reward 1 \
  --actions "tools and approach used"
```
- If the step failed: `--reward -1 --failure-mode "what went wrong"`
- If the saga is finished: add `--done`
- To define the next step in the same call: add
  `--next-slug <slug> --next-prompt "<prompt>"`

### 6. STOP (after complete, DO NOT continue working)
Do NOT make further code changes after running `agentrail complete`.
Any changes after complete are untracked and invisible to the next
session. Future work belongs in the NEXT step, not this one.

## Key Rules

- **Do NOT skip steps** -- the next session depends on accurate tracking
- **Do NOT ask for permission** -- the step prompt is the instruction
- **Do NOT continue working** after `agentrail complete`
- **Commit before complete** -- always commit first, then record completion
- **No step → no work** -- if `agentrail next` shows no step, stop and ask

## CRITICAL: `.agentrail/` directory rules

The `.agentrail/` directory is the durable record of saga and step
history. Treat it like source code. Past incidents in this repo lost
work because these rules were not followed.

- **`.agentrail/` MUST be tracked in git.** Never add it to
  `.gitignore`. If you find it ignored, that is a bug -- unignore it
  and commit the existing contents first.
- **NEVER edit or delete files under `.agentrail/` or
  `.agentrail-archive/` by hand.** No `rm`, `rm -rf`, `mv`, or
  `Write`/`Edit` tool calls on anything inside these directories.
  Always go through agentrail subcommands (`init`, `add`, `begin`,
  `complete`, `abort`, `archive`, `plan`).
- Direct deletion of untracked step files is **unrecoverable** --
  git reflog cannot restore blobs that were never staged. This has
  happened before and lost saga history.
- **Commit `.agentrail/` changes alongside the code changes that
  produced them**, not in a separate "tracking" commit later.
- Before any risky agent operation that touches `.agentrail/` (a
  big run, a rebase, cleaning untracked files), run
  `agentrail snapshot` to save a recoverable copy into the git
  object store.

## Useful Commands

```bash
agentrail status          # Current saga state (read-only)
agentrail history         # All completed steps (read-only)
agentrail plan            # View the plan
agentrail next            # Current step + context

# Maintenance / ad-hoc
agentrail add --slug <slug> --prompt "<prompt>"   # add a step without
                                                  # completing the current one
agentrail abort --reason "..."                    # mark current step blocked
agentrail archive --reason "..."                  # close out a saga
agentrail init <name>                             # start a new saga

# Diagnostics and recovery
agentrail audit                  # report saga-vs-git gaps (markdown)
agentrail audit --emit-commands  # shell script of suggested `add` lines
                                 # for orphan commits (review before run)
agentrail snapshot               # save .agentrail/ into git object store
agentrail snapshot --list        # list snapshot refs
```

If `agentrail audit` reports orphan commits or working-tree drift,
fix it before starting new work. Use `--emit-commands` to scaffold
retroactive `add` lines, then review and run.

**Known artifact:** the commit that initializes a saga (or runs
`agentrail archive` + `agentrail init`) cannot reference itself as
its own backing step, and `agentrail complete` rewrites the step
file with summary and HEAD hash *after* its own commit. So a brand
new saga always has a one-commit "self-orphan" lag and a small
post-complete diff for the most recent step. This is expected, not
a bug -- don't chase it with another bookkeeping commit (you'll
just create the next one).

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
