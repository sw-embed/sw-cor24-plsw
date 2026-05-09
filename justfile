# PL/SW Compiler -- Build System
# Builds plsw compiler (C via tc24r) for COR24 emulator

tc24r_include := env("ORGROOT", env("HOME") / "github/sw-embed") / "sw-cor24-x-tinyc/include"
main_c := "src/main.c"
main_s := "build/plsw.s"
plsw_lgo := "build/plsw.lgo"
linker_dir := "components/linker"
dist_bin := "dist/bin"

# Build the compiler assembly (.s)
build:
    mkdir -p build
    tc24r {{main_c}} -o {{main_s}} -I {{tc24r_include}} -I src

# Assemble the compiler to a shippable .lgo artifact for the
# shared toolchain (mike's orchestrator installs this to the
# COR24 lib path so downstream consumers can `cor24-emu --lgo`
# this directly without re-assembling).
build-lgo: build
    cor24-asm {{main_s}} -o {{plsw_lgo}}

# Build and run interactively
run: build-lgo
    cor24-emu --lgo {{plsw_lgo}} --terminal --echo --speed 0

# Build and run with UART input string (supports \n escapes)
run-input input: build-lgo
    cor24-emu --lgo {{plsw_lgo}} --speed 0 -u "{{input}}"

# Run the PL/SW reg-rs regression suite. Tests live under
# reg-rs/plsw_*.{rgt,out,err}; driver is tests/driver.sh. The
# build-lgo dependency ensures a fresh clone produces the
# compiler artifact before any test fixture invokes pipeline.sh.
# If reg-rs reports "no tests matched", run `just
# test-bootstrap-goldens` once to create the baseline.
test: build-lgo
    ./scripts/test.sh

# (Re)create reg-rs goldens from the current toolchain output.
# Run once when first establishing the baseline, or after an
# intentional behavior change lands. Commit the resulting
# reg-rs/plsw_*.rgt and reg-rs/plsw_*.out files.
test-bootstrap-goldens: build-lgo
    ./scripts/bootstrap-goldens.sh

# Run the linker integration demos (assemble -> meta-gen -> link24
# -> cor24-emu). Separate from `test` because they exercise
# components/linker/ end-to-end and currently surface a
# pre-existing garbled-output regression unrelated to this saga;
# see docs/testing.md. demo-plsw-modular.sh needs build/plsw.s
# to compile its .plsw fixtures, so we depend on build-lgo here
# too.
test-linker: build-lgo
    ./components/linker/tests/demo-fixup.sh
    ./components/linker/tests/demo-plsw-modular.sh

# Smoke-run the compiler binary interactively (no specific input,
# 100M cycle ceiling). Useful for "does the compiler still run at
# all?", not for regression. The real regression is `just test`.
smoke: build-lgo
    cor24-emu --lgo {{plsw_lgo}} --terminal --speed 0 -n 100000000

# Compile and run a .plsw program end-to-end
# Usage: just pipeline examples/hello.plsw
#        just pipeline examples/greet.msw examples/hello_macro.plsw
pipeline *args: build-lgo
    ./scripts/pipeline.sh {{args}}

# Compile a .plsw, save .s, and run with --dump for memory inspection
# Output: build/out.s (assembly) and build/run-dump.txt (memory dump)
pipeline-dump *args: build-lgo
    ./scripts/pipeline-dump.sh {{args}}

# Compile hello_macro example (macro demo)
hello-macro: build-lgo
    ./scripts/pipeline-dump.sh examples/greet.msw examples/hello_macro.plsw

# Compile chain example (control block demo with .msw includes)
chain: build-lgo
    ./scripts/pipeline-dump.sh include/cvt.msw include/ascb.msw include/asxb.msw include/tcb.msw examples/chain.plsw

# Stage Layer 1 native binaries (link24, meta-gen) for the
# shared toolchain orchestrator. Builds release binaries and
# copies them to dist/bin/ at a stable relative path. mike's
# tools/build-all picks them up from there post-relay.
install-layer1:
    cd {{linker_dir}} && cargo build --release
    mkdir -p {{dist_bin}}
    cp {{linker_dir}}/target/release/link24 {{dist_bin}}/link24
    cp {{linker_dir}}/target/release/meta-gen {{dist_bin}}/meta-gen
    @echo "Layer 1 binaries staged at {{dist_bin}}/{link24,meta-gen}"

# Clean build artifacts
clean:
    rm -f build/*.s build/*.lgo build/*-combined.plsw build/*-dump.txt
