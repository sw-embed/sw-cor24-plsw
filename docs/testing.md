# Testing PL/SW

PL/SW's regression suite uses [`reg-rs`](https://github.com/sw-embed/reg-rs)
-- the same golden-output convention used across other sw-cor24-*
language projects (sw-cor24-basic, etc.). Each test is a
(.rgt, .out, .err) triple under `reg-rs/`: `.rgt` is the test
metadata (command + exit code), `.out` is the captured golden
stdout, and `.err` is the captured golden stderr. Re-running a
test executes its command and diffs the live output against the
golden -- any mismatch in stdout, stderr, or exit code is a
regression. (`.tdb` files are reg-rs's internal SQLite index,
regenerated from the triples on demand, and are gitignored.)

## Layout

```
tests/
  driver.sh                   per-case dispatcher; calls scripts/pipeline.sh
                              with the right .msw includes for each example
reg-rs/
  plsw_<case>.rgt             test metadata (TOML: command + exit_code)
  plsw_<case>.out             golden stdout
  plsw_<case>.err             golden stderr
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
behavior of any `.plsw` example changed. It depends on
`just build-lgo` so a fresh clone produces `build/plsw.lgo` before
any test fixture invokes `pipeline.sh`; running `just test`
end-to-end on a clean tree is the supported "is everything wired
up" check. `just test-linker` is separate because it currently
surfaces a pre-existing failure (see Blockers below) that's
unrelated to ordinary PL/SW changes; it depends on `build-lgo`
too because `demo-plsw-modular.sh` needs the compiler artifact.
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
captured, and the resulting `reg-rs/plsw_<case>.{rgt,out,err}`
triples land on disk.

### Operator review (REQUIRED)

**Captured goldens are not authoritative until a human has
reviewed them.** Reg-rs faithfully reproduces whatever was
captured -- including a demo that fails to compile, an OOM, or
any other "broken" state. If the bootstrap captures a broken
state, every subsequent `just test` will report green because
the broken state is reproducible.

The `plsw_record` incident (2026-05-09) is the worked example.
`examples/record.plsw` was using `ADDR('literal')`, which the
codegen rejects with `ADDR requires a variable, field, or array
element`. Bootstrap captured `exit_code = 1` and an empty `.out`,
which then "passed" forever -- masking a real demo regression
nobody noticed until they tried to run the demo.

Before `git add reg-rs/...`, open each
`reg-rs/plsw_<case>.{rgt,out,err}` triple and confirm:

* **`.rgt`**: `exit_code = 0` (unless the demo's intended state
  really is non-zero -- rare; document why here in this file).
* **`.out`**: non-empty and contains the expected program output.
* **`.err`**: diagnostics look clean (no unexpected stack
  traces, no compilation errors).

`scripts/bootstrap-goldens.sh` catches the easy case (non-zero
exit code) and aborts with a non-zero exit unless
`--allow-failures` is passed. That guard is a backstop, not a
substitute for review.

Commit all three files in the triple:

```sh
git add reg-rs/plsw_*.rgt reg-rs/plsw_*.out reg-rs/plsw_*.err
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
5. `git add reg-rs/plsw_<new-case>.{rgt,out,err}` and commit.

## Linker tests

`components/linker/tests/demo-fixup.sh` and
`components/linker/tests/demo-plsw-modular.sh` exercise the
separate-compilation pipeline (assemble -> meta-gen -> link24 ->
cor24-emu). They're run via `just test-linker` rather than
`just test` because they use hand-written `.s` fixtures under
`components/linker/tests/fixtures-{fixup,plsw}/`, not the
reg-rs convention.

Status as of this saga (against the May-7 toolchain on PATH):

| Demo | Status | Notes |
|---|---|---|
| `demo-plsw-modular.sh` | ✅ PASS | end-to-end .plsw -> .lgo via PL/SW + link24 + meta-gen + cor24-emu; expected output `HI` matches |
| `demo-fixup.sh` | ❌ garble | UART output is `~eU\t:` repeated 3x instead of the expected `main:enter\n...main:leave` sequence |

`demo-fixup.sh` runs cleanly (no script error, exits 0), assembles
all four hand-written modules, links them, loads via
`cor24-emu --load-binary`, and reads stable garbled bytes from the
program's UART writes. The script's `awk` filter strips the
expected header (`Loaded N bytes...`/`Entry point:`/`Running...`)
and isolates the program's UART traffic, leaving:

```
~eU	:
z{|'~eU	:
~eU	:
```

Hex view of the bytes: `0x7E 0x65 0x55 0x09 0x3A 0x0A` repeated
with minor variation. The expected output is ASCII
`main:enter\nliba:enter\nutil:inc\nliba:leave\nlibb:enter\nutil:inc\nlibb:leave\nmain:leave\n`,
none of which appears.

Probable causes (not investigated in this saga):

1. **Stale hand-written `.s` fixtures.** `fixtures-fixup/{main,liba,libb,util}.s`
   may target older addressing or directive conventions. The
   modular-plsw path (which compiles fresh `.plsw` to `.s` via
   `pl-sw`) works correctly, so the toolchain itself is fine —
   the bug is upstream in the fixture text.
2. **`la <reg>, <imm>` resolution drift.** If the fixtures use
   absolute-address `la` forms that `cor24-asm`'s newer encoding
   handles differently, or if the `--base-addr` semantics in
   pass-2 reassembly produce different bytes than the older
   `cor24-run --assemble` did, the resulting program would jump
   into wrong code/data. The 3-cycle repetition in the garble
   (each module entering and outputting the same bad string)
   matches what you'd see if all three modules' `_UART_PUTS` calls
   resolve to the same wrong data address.
3. **`fixtures-fixup` was always broken** and the comment-stated
   "Expected output" was aspirational rather than empirical. Git
   blame on the fixture files would establish whether they ever
   produced the `main:enter` sequence.

This is a separate blocker from the PL/SW goldens work — the
production .plsw compilation path is exercised by the modular
demo and works. Out of scope for this saga; capture for a future
investigation.
