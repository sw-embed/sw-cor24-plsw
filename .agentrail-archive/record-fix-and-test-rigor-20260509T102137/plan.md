# Record Demo Fix + Test-Rigor Saga

Two related issues:

1. **`examples/record.plsw` doesn't compile.** It uses
   `ADDR('literal')` for the row labels, but the current codegen
   rejects inline string literals to `ADDR()` ("ADDR requires a
   variable, field, or array element"). Already fixed in the
   web-ui clone (`web-sw-cor24-plsw/src/demos.rs`); needs
   retrofitting here. While we're at it, also use a `PRINT_STR`
   helper that walks a NUL-terminated label via `UART_PUTCHAR`,
   so the label and value land on the same line (`UART_PUTS`
   appends a newline, splitting label from value otherwise).

2. **The reg-rs golden for `plsw_record` was bootstrapped while
   the demo was broken.** `reg-rs/plsw_record.rgt` records
   `exit_code = 1` and `.out` is empty -- exactly what a failed
   compile produces. So `just test` reports green even though
   `record.plsw` fails to compile and produces no output.

Issue 2 is a process failure on the operator's part, not a
reg-rs bug. The fix is to **require human inspection of golden
files before commit** -- and to make the bootstrap script
surface anomalies (non-zero exit codes, empty stdout) so they
can't slip through.

## Goal

After this saga: `examples/record.plsw` compiles and produces
the expected `X = 100 / Y = 200 / P->X = 100 / P->Y = 200 /
Sum = 300` output; reg-rs goldens for all 15 cases are
re-validated with operator review; bootstrap script flags
anomalies; `docs/testing.md` documents the inspection
requirement explicitly.

## Steps

1. **record-fix-and-test-rigor**. Single step:
   - Rewrite `examples/record.plsw` using pre-declared CHAR
     labels and a `PRINT_STR` helper. Match the
     `web-sw-cor24-plsw` shape but with the original 100/200
     values to preserve recognizability of the demo's output.
   - Update `scripts/bootstrap-goldens.sh`: after each
     `reg-rs create`, summarize the captured exit_code and
     stdout-line-count; abort the run (non-zero exit) if any
     case has a non-zero exit code, unless the operator passes
     a `--allow-failures` flag explicitly. Print a clear
     "REVIEW BEFORE COMMITTING" banner.
   - Update `docs/testing.md`: add an "Operator review" section
     making it explicit that goldens are *not* trustworthy until
     a human has inspected each `.out`/`.err`/`.rgt` triple for
     sanity. Document the bootstrap-script's anomaly checks.
     Note the `plsw_record` incident as the worked example.
   - Re-bootstrap reg-rs goldens for all 15 cases. Inspect each
     before committing. Verify each `.out` is non-empty and
     matches expectations; verify each `.rgt` has `exit_code = 0`.
   - Verify `just test` 15/15 green from a clean rebuild.
