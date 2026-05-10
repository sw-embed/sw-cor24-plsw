# Streaming Emit Saga

Per `tools/briefs/dcpls-streaming-emit.md` (agent-drafted by
dcsno). Replace PL/SW's 256 KB `emit_buf` accumulator with a
small coalescing flush buffer (Option B from the brief), so:

* SRAM frees ~252 KB for AST / symbol / future use.
* `.s` output is unbounded -- removes the 256 KB ceiling that
  blocks dcsno's `saga-expr-completeness` step 003 (sno_exec.s
  is at 99.96% of the ceiling, 114 bytes free).
* Library-module `.s` files (which `.zero N` codegen can't
  shrink because they're code-only) get the same headroom.

## Prerequisite already verified

`grep -rn 'emit_buf\|emit_pos\|EMIT_BUF_SIZE' include/ src/`
returned 13 references all in `src/emit.h`, plus 110+ caller
references to `emit_output()` and 1 to `emit_length()`. Two
non-trivial readers found (and reported to mike before starting):

1. **`emit_output()`** -- production: `compile_program`
   returns it; `main()`'s compile-mode handler does
   `uart_putstr(out)` to dump the `.s`. Tests: 110+ callers use
   it for `uart_putstr` and `str_find` assertions.

2. **`emit_length()`** -- 1 test caller.

The streaming change *re-routes* the production reader (UART
output already streams; the trailing dump becomes redundant)
and *changes the contract* of the test reader (now reflects
only the unflushed tail). Neither requires a "different
lowering" beyond plumbing the flush + adjusting the production
caller.

## Approach (Option B from the brief)

* `EMIT_BUF_SIZE = 4096` (was 262144).
* `emit_flush()`: drain `emit_buf[0..emit_pos]` to UART via
  `uart_putchar`; reset `emit_pos = 0`.
* `emit_ch`: if `emit_pos >= EMIT_BUF_SIZE`, call `emit_flush()`
  before writing.
* `emit_output()` keeps returning `emit_buf` -- contract
  becomes "unflushed tail only." Existing tests that emit < 4 KB
  fragments work unchanged. Test docstring updated to flag the
  invariant.
* `compile_program()` calls `emit_flush()` at end (still
  returns `emit_output()` so the NULL-on-failure contract
  holds).
* `main()`'s compile-mode handler drops the
  `uart_putstr(out)` line -- bytes already streamed; printing
  again would duplicate the trailing < 4 KB chunk.

## Tests

1. `just test` 15/15 green (existing reg-rs gate; .bin
   byte-identical even if .s flush-boundary whitespace differs;
   reg-rs compares stdout/stderr text -- need to verify the
   pipeline.sh capture path is robust to flush-boundary changes
   in the streamed output).
2. Compile suites in src/main.c: most produce < 4 KB output;
   should still pass. Re-run `just smoke` and a few specific
   suites (16 = static data, 29 = macro expansion) to verify.
3. Synthetic large-output test: compile a `.plsw` whose `.s`
   exceeds the old 256 KB ceiling. Confirm the resulting `.bin`
   loads cleanly via `cor24-asm`. (New reg-rs case
   `plsw_emit_unbounded` per the brief.)

## Steps

1. **streaming-emit**. Single step:
   - Modify `src/emit.h`: shrink buffer, add `emit_flush`,
     update `emit_ch` to flush on full, update `emit_output()`'s
     docstring to flag the new contract.
   - Modify `src/main.c`: add `emit_flush()` to the end of
     `compile_program`; remove the `uart_putstr(out)` in
     `main()`'s compile-mode handler.
   - Verify `just test` 15/15.
   - Verify compile-suite tests (16, 29 etc.) don't regress.
   - Add a synthetic large-output test if practical.
   - Re-run `just clean && just build-lgo`. Note new SHA in
     CHANGES.md so mike knows to rebuild + reinstall.
   - Commit + complete + mark-pr.

## What does NOT go in this PR

(Per the brief.)

* No allocation strategy change elsewhere.
* No `cor24-asm`, `link24`, or downstream change.
* No macro / codegen logic change beyond the emit primitives.
* No `.zero N` codegen change -- that fix keeps its win.
