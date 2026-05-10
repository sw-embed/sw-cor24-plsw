# Shrink plsw.lgo

The compiled PL/SW compiler (`build/plsw.lgo`) is 1.7 MB — far
larger than this language's "tiny integer subset of PL/I" warrants
for an embedded target. Storage waste here is gating dcsno's
`saga-expr-completeness` and, downstream, dcftn. This saga reduces
both static-storage waste and code size to free SRAM headroom and
speed builds.

## Corrected baseline understanding

Earlier analysis claimed tc24r emits one COR24 word per `char[]`
slot ("3× tax"). That was wrong: `sizeof(char) == 1` in tc24r and
chars are packed 3-per-word in `.word` directives. The original
brief at `tools/briefs/dcpls-dynamic-memory-architecture.md` carried
the same incorrect assumption in its constraint #1. The true
physical sizes are ~1× the C array length, not 3×.

So:

* `emit_buf[262144]` (pre-streaming) was ~256 KB physical, not
  786 KB. The streaming-emit saga's win was real but smaller than
  the brief implied.
* `chunk_storage[16 * 65536]` proposed in Phase 2 = 1 MB physical,
  not 3 MB — fits in SRAM, but at 100% of budget, so still wants
  parameter tuning.
* No tc24r ABI change is needed. The brief's "out of scope" item
  on 24-bit char packing is moot.

## Scope

This saga is the umbrella plan; individual phases ship as their
own sagas. The plan and detailed phase breakdown live in
`docs/shrink-lgo-size.md` (written by step 1 of this saga).

## Steps

1. **plan-and-baseline**: write `docs/shrink-lgo-size.md` with
   the corrected analysis, the multi-phase roadmap, and the
   measured baseline (component sizes by category). Update the
   architecture brief's constraint #1 inline correction note.
   Commit doc + any measurement scripts.

Subsequent phases (test split, chunk allocator, lint, etc.) will
be added as separate sagas based on what the baseline reveals.
