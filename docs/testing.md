# Testing PL/SW

PL/SW's regression suite uses [`reg-rs`](https://github.com/sw-embed/reg-rs)
-- the same golden-output convention used across other sw-cor24-*
language projects (sw-cor24-basic, etc.). Each test is a (.rgt,
.out) pair under `reg-rs/`: `.rgt` is the test metadata (command,
timeout, exit code, description) and `.out` is the captured
golden stdout+stderr. Re-running a test executes its command and
diffs the live output against the golden -- any mismatch is a
regression.

## Layout

```
tests/
  driver.sh                   per-case dispatcher; calls scripts/pipeline.sh
                              with the right .msw includes for each example
reg-rs/
  plsw_<case>.rgt             test metadata (TOML)
  plsw_<case>.out             golden output (text)
  *.tdb / *.tdb.lock          reg-rs SQLite index (gitignored)
scripts/
  test.sh                     wraps `reg-rs run -p plsw_ --parallel`
  bootstrap-goldens.sh        wraps `reg-rs create -t plsw_<case> -c ...`
                              for each case; used to (re)create goldens
```

## Cases (one per `.plsw` example)

| Case | Fixture | Includes |
|---|---|---|
| `hello`         | `examples/hello.plsw`         | -- |
| `led`           | `examples/led.plsw`           | -- |
| `loop`          | `examples/loop.plsw`          | -- |
| `record`        | `examples/record.plsw`        | -- |
| `define`        | `examples/define.plsw`        | -- |
| `select_demo`   | `examples/select_demo.plsw`   | -- |
| `select_nested` | `examples/select_nested.plsw` | -- |
| `macro`         | `examples/macro.plsw`         | `examples/system.msw` |
| `hello_macro`   | `examples/hello_macro.plsw`   | `examples/greet.msw` |
| `chain`         | `examples/chain.plsw`         | `include/{cvt,ascb,asxb,tcb}.msw` |

Each case is exposed as a reg-rs test named `plsw_<case>`. The
`tests/driver.sh <case>` script encodes the include list per case
and invokes `scripts/pipeline.sh` with the right argument order.

## Running the suite

```sh
just test                    # run all reg-rs tests; non-zero exit on regression
just test-linker             # run components/linker/tests/demo-*.sh end-to-end
just smoke                   # interactive compiler smoke (was the old `just test`)
```

`just test` is the regression gate -- if it's green, no observable
behavior of any `.plsw` example changed. `just test-linker` is
separate because it currently surfaces a pre-existing failure
(see Blockers below) that's unrelated to ordinary PL/SW changes.
`just smoke` is a manual sanity check that just runs the compiler
binary interactively for 100M cycles.

## Bootstrapping goldens

The first time the suite is established (or after an intentional
behavior change lands), run:

```sh
just test-bootstrap-goldens
```

This invokes `reg-rs create -t plsw_<case> -c './tests/driver.sh <case>'`
for each case. The command is run, its stdout+stderr+exit code
captured, and the resulting `reg-rs/plsw_<case>.{rgt,out}` files
land on disk. Commit them:

```sh
git add reg-rs/plsw_*.rgt reg-rs/plsw_*.out
```

Subsequent `just test` runs compare against those committed
baselines. To rebase a single test after an intentional change,
use `reg-rs rebase plsw_<case>` (re-runs the command and accepts
the new output as the baseline).

## Adding a new case

1. Add a new `.plsw` (and any `.msw` includes) under `examples/`
   or `include/`.
2. Add a `case` arm in `tests/driver.sh` mapping the case name to
   the right pipeline.sh argument list.
3. Add the case to the `cases=(...)` list in
   `scripts/bootstrap-goldens.sh`.
4. Run `just test-bootstrap-goldens` to capture the golden.
5. `git add reg-rs/plsw_<new-case>.{rgt,out}` and commit.

## Linker tests

`components/linker/tests/demo-fixup.sh` and
`components/linker/tests/demo-plsw-modular.sh` exercise the
separate-compilation pipeline (assemble -> meta-gen -> link24 ->
cor24-emu). They're run via `just test-linker` rather than
`just test` because:

1. They use hand-written `.s` fixtures under
   `components/linker/tests/fixtures-{fixup,plsw}/`, not the
   reg-rs convention.
2. They currently still depend on `cor24-run --assemble --base-addr`
   for pass-2 reassembly (cor24-asm 0.1.0 has no `--base-addr` --
   tracked in the dcxas brief at `tools/briefs/dcxas-cor24-asm-base-addr.md`).
3. **Output garble (pre-existing).** `demo-fixup.sh` produces
   garbled UART text instead of the expected
   `main:enter\nliba:enter\n...` sequence. The script runs end-to-end
   and exits cleanly, but the linked binary's runtime output
   doesn't match what the script's header comment promises. Root
   cause not yet investigated; likely a stale fixture, a
   relocation bug in link24/meta-gen, or a runtime-memory
   assumption that drifted post-cor24-emu install. Treat the
   linker demos as exercise-only until that's diagnosed.

## Current blockers (test bootstrap is gated on these)

`just test-bootstrap-goldens` depends on `just build-lgo` working,
which requires `tc24r` to compile `src/main.c` cleanly. Two
distinct tc24r limitations are blocking that today:

1. **`[A * B]` array sizes** (`src/macro.h:387`). tc24r 0.x
   doesn't accept binary expressions in array size declarations.
   Fix in flight: dcxtc/`pr/array-size-expressions`. Brief:
   `devgroup/tools/briefs/dcxtc-array-size-expressions.md`.

2. **Adjacent string-literal concatenation** (`src/main.c`, ~265
   continuation lines across the test fixtures). tc24r 0.x errors
   on `"abc" "def"` -- standard C since C89. Fix in flight:
   dcxtc/`pr/string-literal-concatenation`. Brief:
   `devgroup/tools/briefs/dcxtc-string-literal-concatenation.md`.

We are dogfooding the compiler -- no PL/SW-side workarounds. Both
fixes land in tc24r; once mike installs the new binary,
`just build` succeeds and `just test-bootstrap-goldens` lights up.

After that, this saga's follow-up is one short PR:
1. `just test-bootstrap-goldens`
2. `git add reg-rs/plsw_*.{rgt,out}`
3. Commit, mark-pr, relay.

From then on, every `pr/*` in this repo runs `just test` as a
gate.
