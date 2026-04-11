# PL/SW Maintenance Saga

Ongoing maintenance saga for the PL/SW compiler after the v1
(plsw-compiler, 67 steps) and v2 (plsw-modular, 5 steps) sagas
completed. This saga has two phases:

## Phase 1 -- Retroactive bookkeeping

Bind the post-modular orphan commits (Apr 7 - Apr 10 2026) to step
records so `agentrail audit` reports a clean repo. These were
ad-hoc commits made between sagas because the old CLAUDE.md did
not tell agents to stop when no current step exists. The CLAUDE.md
gap is now fixed.

Orphan commits to absorb:
- 72a7111 fix(plsw): raise %DEFINE table to 512 (Apr 7 09:57)
- 85a58f3 docs(readme): add Projects Using PL/SW section (Apr 7 17:25)
- e43f01a feat(runtime): add _UART_GETCHAR stub for UART RX (Apr 8 20:05)
- e7d4805 feat(select): add SELECT/WHEN/OTHERWISE control flow (Apr 10 18:29)
- b63ab15 fix(plsw): make OTHERWISE optional in SELECT (Apr 10 20:52)

## Phase 2 -- Ongoing maintenance

Track future bug fixes, small features, doc updates, and any other
work that does not warrant a dedicated saga. Each step is one
focused change. When a coherent multi-step initiative comes along
(e.g. another major feature like modular compilation was), archive
this saga and start a dedicated one.

Open work as of saga creation:
- GitHub issue #33: long IF/ELSE-IF chain limit -- documented as
  known limitation, no fix planned. Tracking ticket only.

## Rules

- Each step is one commit (or one cohesive set of commits when work
  needs to be staged).
- Use `agentrail add` for retroactive entries (Phase 1) since the
  commits already exist.
- Use the normal `next` / `begin` / `complete` flow for forward work.
- Stop and ask the user before doing any code work if `agentrail
  next` reports no current step.
