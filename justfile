# PL/SW Compiler -- Build System
# Builds plsw compiler (C via tc24r) for COR24 emulator

tc24r_include := env("HOME") / "github/sw-vibe-coding/tc24r/include"
main_c := "src/main.c"
main_s := "build/plsw.s"

# Build the compiler
build:
    tc24r {{main_c}} -o {{main_s}} -I {{tc24r_include}} -I src

# Build and run interactively
run: build
    cor24-run --run {{main_s}} --terminal --echo --speed 0

# Build and run with UART input string (supports \n escapes)
run-input input: build
    cor24-run --run {{main_s}} --speed 0 -u "{{input}}"

# Build and run with cycle limit (for testing)
test: build
    cor24-run --run {{main_s}} --terminal --speed 0 -n 100000000

# Compile and run a .plsw program end-to-end
# Usage: just pipeline examples/hello.plsw
#        just pipeline examples/greet.msw examples/hello_macro.plsw
pipeline *args: build
    ./scripts/pipeline.sh {{args}}

# Clean build artifacts
clean:
    rm -f build/*.s
