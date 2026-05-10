Introduce accessor macros for the AST parallel arrays and
mechanically migrate all ~391 callsites. **No storage change in
this step** -- the macros initially still resolve to the existing
parallel arrays. This step proves the macro layer compiles and
all tests pass before storage gets disturbed in the next step.

Steps:

1. In `src/ast.h`, rename the parallel-array storage from
   `nd_kind` to `nd_kind_arr` (and similarly for the other 9
   fields). Add accessor macros (or static-inline functions, if
   tc24r handles them well -- macros are safer):
   ```c
   #define nd_kind(i)  nd_kind_arr[i]
   #define nd_type(i)  nd_type_arr[i]
   #define nd_stor(i)  nd_stor_arr[i]
   #define nd_level(i) nd_level_arr[i]
   #define nd_left(i)  nd_left_arr[i]
   #define nd_right(i) nd_right_arr[i]
   #define nd_next(i)  nd_next_arr[i]
   #define nd_ival(i)  nd_ival_arr[i]
   #define nd_line(i)  nd_line_arr[i]
   #define nd_name(i)  nd_name_arr[i]
   ```
   Also update `nd_alloc()` and the other helpers in `ast.h` to
   use the new accessor form (they currently use `nd_kind[i]`
   directly).

2. Update **all** `nd_*[i]` callsites across the project to use
   `nd_*(i)` form. Files to touch (from grep
   `nd_kind\[\|nd_type\[\|...` over `src/*.h src/*.c`):
   - `src/ast.h` (besides the renames in step 1)
   - `src/types.h`
   - `src/parser.h`
   - `src/layout.h`
   - `src/codegen.h`
   - `src/test_main.c`

   Verify with grep that **no `nd_kind\[\|nd_type\[\|nd_stor\[
   \|nd_level\[\|nd_left\[\|nd_right\[\|nd_next\[\|nd_ival\[
   \|nd_line\[\|nd_name\[` matches remain** in source files.
   (The new macro form is `nd_kind(i)`, not `nd_kind[i]`.)

3. Treat assignment vs read symmetrically: `nd_kind(i) = X` works
   if the macro expands to an lvalue. With `nd_kind(i)` ->
   `nd_kind_arr[i]`, both reads and writes work because
   `nd_kind_arr[i]` is an lvalue.

4. Build:
   - `just clean && just build-lgo` -- production must succeed.
   - `just build-test-lgo` -- test must succeed.
   - `just test` -- reg-rs must remain 15/15 green.
   - Run plsw_test.lgo suite #37 (chunk tests) -- still 0
     errors.
   - Optionally run a few ast/parser/codegen suites in
     plsw_test.lgo to spot-check the macro expansion didn't
     break anything subtle.

5. Commit on `feat/ast-accessors`. Include `.agentrail/`. Stop
   at commit (no push).

6. `agentrail complete --summary "..." --reward 1
   --actions "..."`. Then `dg-mark-pr` and STOP.

## Risks to watch

* If a macro expansion lands inside a sizeof() or other
  syntactic-sensitive context, the parens around `(i)` in the
  macro body matter. Use defensive parens consistently:
  `#define nd_kind(i) (nd_kind_arr[(i)])`.
* If `nd_kind` and `nd_kind(i)` collide (e.g. someone took the
  address `&nd_kind[0]` or passed `nd_kind` as an array
  pointer), the macro form breaks. Grep for `nd_kind` (no
  bracket) and `&nd_kind\[` to find these and rewrite as
  needed (or expose `nd_kind_arr` directly in those rare
  cases).

## Out of scope

* Switching storage to chunks -- next step
  (`ast-chunk-storage`).
* Removing `NODE_POOL_MAX` -- still defines the static array
  sizes after this step. Removed in `ast-chunk-storage`.
