Phase 0 measurement step. Add `#ifdef MEASURE` instrumentation to
`nd_alloc()` to track peak node usage; build a measurement binary;
run across representative inputs; capture peak counts; decide
`CHUNK_MAX` for step 3.

Steps:

1. Add measurement to `src/ast.h`:
   ```c
   #ifdef MEASURE
   int peak_nd;
   void measure_ast_dump(void) {
       uart_putstr("AST peak: ");
       print_int(peak_nd);
       uart_putchar(10);
   }
   #endif
   ```
   Inside `nd_alloc()`, after incrementing `nd_count`, add:
   ```c
   #ifdef MEASURE
   if (nd_count > peak_nd) peak_nd = nd_count;
   #endif
   ```
   Inside `ast_init()`, reset `peak_nd = 0` under the same guard.

2. Build a one-off measurement binary by adding `#define MEASURE`
   at the top of a copy of `src/main.c` (or inject via tc24r `-D`
   if it works for this case -- prior saga noted tc24r silently
   ignores `-D` for `#ifdef`, so a source-level `#define MEASURE`
   in a temporary file is the reliable path). Add a call to
   `measure_ast_dump()` at the end of `compile_program()` (also
   under `#ifdef MEASURE`).

3. Run the measurement binary against representative inputs:
   - `tests/inputs/hello.plsw` (smallest realistic)
   - `examples/hello_macro.plsw` (macro-heavy if present)
   - The largest `.plsw` fixture under `reg-rs/` or `tests/`
   - If the production compiler can self-compile (i.e. compile
     `src/main.c` itself via `cor24-emu` -- unlikely on a
     production cor24-emu run; document if not feasible)

   For each input, capture the peak AST node count.

4. Write `docs/memory-audit-2026-05-10.md` with a table:
   | Input | Peak nd_count | % of NODE_POOL_MAX (12288) | Fits in 16 chunks (2,176)? |

5. Decide `CHUNK_MAX`. If the largest measured peak is well
   under 2,176 (say <1,500), keep `CHUNK_MAX=16`. If between
   1,500 and 2,176, keep 16 with a note that the next saga
   may need to bump. If above 2,176, bump `CHUNK_MAX` in
   `src/chunk.h` to ceil(peak / 136) × safety-margin (e.g.
   `peak / 136 * 1.5`, rounded up). Document the decision in
   the audit doc.

6. Remove the measurement code from `src/ast.h` (revert) so
   production code is unchanged. The audit doc is the durable
   artifact, not the instrumentation. (We can re-add `#ifdef
   MEASURE` when wanted later; this step's deliverable is the
   numbers.)

7. Commit on `feat/measure-ast`. Include `.agentrail/` files +
   any `src/chunk.h` adjustment + the new
   `docs/memory-audit-2026-05-10.md`.

8. `agentrail complete --summary "..." --reward 1
   --actions "..."`. Then `dg-mark-pr` and STOP.

## Out of scope

* Actually migrating the AST onto chunks (next step:
  `ast-accessors`).
* Touching other pools' `_MAX` values.
* Permanent measurement instrumentation (`#ifdef MEASURE`
  added then reverted -- the audit doc is the durable
  artifact).
