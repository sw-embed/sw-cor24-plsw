// link24 -- COR24 module linker
//
// Reads .bin + .meta file pairs, resolves cross-module references
// via FIXUP patching, and writes a single combined binary.
//
// See docs/linker-design.md for the full design.

use std::collections::HashMap;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process;

// --- Meta file structures ---

struct Fixup {
    offset: u32,
    symbol: String,
}

struct Export {
    symbol: String,
    offset: u32,
}

struct ModuleMeta {
    name: String,
    exports: Vec<Export>,
    fixups: Vec<Fixup>,
}

struct LinkedModule {
    meta: ModuleMeta,
    binary: Vec<u8>,
    base: u32,
}

// --- Meta file parser ---

fn parse_meta(path: &Path) -> ModuleMeta {
    let content = fs::read_to_string(path)
        .unwrap_or_else(|e| die(&format!("cannot read {}: {}", path.display(), e)));

    let mut name = String::new();
    let mut exports = Vec::new();
    let mut fixups = Vec::new();

    for (line_num, raw_line) in content.lines().enumerate() {
        let line = match raw_line.find(';') {
            Some(pos) => &raw_line[..pos],
            None => raw_line,
        };
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.is_empty() {
            continue;
        }

        match parts[0] {
            "MODULE" => {
                if parts.len() < 2 {
                    die(&format!("{}:{}: MODULE requires a name", path.display(), line_num + 1));
                }
                name = parts[1].to_string();
            }
            "EXPORT" => {
                if parts.len() < 3 {
                    die(&format!("{}:{}: EXPORT requires symbol and offset", path.display(), line_num + 1));
                }
                let offset = parse_hex(parts[2])
                    .unwrap_or_else(|| die(&format!("{}:{}: bad offset '{}'", path.display(), line_num + 1, parts[2])));
                exports.push(Export {
                    symbol: parts[1].to_string(),
                    offset,
                });
            }
            "FIXUP" => {
                if parts.len() < 3 {
                    die(&format!("{}:{}: FIXUP requires offset and symbol", path.display(), line_num + 1));
                }
                let offset = parse_hex(parts[1])
                    .unwrap_or_else(|| die(&format!("{}:{}: bad offset '{}'", path.display(), line_num + 1, parts[1])));
                fixups.push(Fixup {
                    offset,
                    symbol: parts[2].to_string(),
                });
            }
            _ => {
                die(&format!("{}:{}: unknown directive '{}'", path.display(), line_num + 1, parts[0]));
            }
        }
    }

    if name.is_empty() {
        die(&format!("{}: missing MODULE directive", path.display()));
    }

    ModuleMeta { name, exports, fixups }
}

fn parse_hex(s: &str) -> Option<u32> {
    let s = s.trim();
    if let Some(hex) = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        u32::from_str_radix(hex, 16).ok()
    } else {
        u32::from_str_radix(s, 16).ok().or_else(|| s.parse::<u32>().ok())
    }
}

// --- Linker core ---

fn link(modules: &mut [LinkedModule]) -> Vec<u8> {
    // Step 1: Assign base addresses sequentially
    let mut addr: u32 = 0;
    for m in modules.iter_mut() {
        m.base = addr;
        addr += m.binary.len() as u32;
    }

    let total_size = addr as usize;
    if total_size > 0x100000 {
        eprintln!("warning: combined binary is {} bytes, exceeds 1 MB SRAM", total_size);
    }

    // Step 2: Build global symbol table
    let mut symbols: HashMap<String, u32> = HashMap::new();
    for m in modules.iter() {
        for exp in &m.meta.exports {
            let resolved = m.base + exp.offset;
            if symbols.contains_key(&exp.symbol) {
                die(&format!(
                    "duplicate export '{}' in module '{}' (already defined). \
                     Library modules must not emit data for shared globals -- \
                     use %DEFINE LIBRARY in library .plsw sources.",
                    exp.symbol, m.meta.name
                ));
            }
            symbols.insert(exp.symbol.clone(), resolved);
        }
    }

    // Step 3: Concatenate binaries into output buffer
    let mut output = vec![0u8; total_size];
    for m in modules.iter() {
        let base = m.base as usize;
        output[base..base + m.binary.len()].copy_from_slice(&m.binary);
    }

    // Step 4: Apply fixups
    for m in modules.iter() {
        for fixup in &m.meta.fixups {
            let target = match symbols.get(&fixup.symbol) {
                Some(&addr) => addr,
                None => die(&format!(
                    "unresolved symbol '{}' in module '{}' (FIXUP at 0x{:04X})",
                    fixup.symbol, m.meta.name, fixup.offset
                )),
            };

            // The fixup offset points to the la instruction's opcode byte.
            // Patch bytes +1..+3 with the resolved address (little-endian).
            let patch_addr = (m.base + fixup.offset) as usize;
            if patch_addr + 4 > total_size {
                die(&format!(
                    "FIXUP at 0x{:04X} in module '{}' is out of range",
                    fixup.offset, m.meta.name
                ));
            }

            output[patch_addr + 1] = (target & 0xFF) as u8;
            output[patch_addr + 2] = ((target >> 8) & 0xFF) as u8;
            output[patch_addr + 3] = ((target >> 16) & 0xFF) as u8;
        }
    }

    output
}

fn write_map(path: &Path, modules: &[LinkedModule]) {
    let mut f = fs::File::create(path)
        .unwrap_or_else(|e| die(&format!("cannot create {}: {}", path.display(), e)));

    writeln!(f, "; program.map -- generated by link24").unwrap();
    writeln!(f, "; Symbol                     Address    Module").unwrap();

    let mut entries: Vec<(&str, u32, &str)> = Vec::new();
    for m in modules {
        for exp in &m.meta.exports {
            entries.push((&exp.symbol, m.base + exp.offset, &m.meta.name));
        }
    }
    entries.sort_by_key(|e| e.1);

    for (sym, addr, module) in entries {
        writeln!(f, "{:<28} {:06X}     {}", sym, addr, module).unwrap();
    }
}

// --- CLI ---

fn die(msg: &str) -> ! {
    eprintln!("link24: error: {}", msg);
    process::exit(1);
}

struct Args {
    entry_module: String,
    module_names: Vec<String>,
    dir: PathBuf,
    output: PathBuf,
    map_output: Option<PathBuf>,
}

fn parse_args() -> Args {
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 2 || args.iter().any(|a| a == "-h") {
        print_usage();
        process::exit(0);
    }

    if args.iter().any(|a| a == "--help") {
        print_help();
        process::exit(0);
    }

    let mut entry_module = String::new();
    let mut module_names = Vec::new();
    let mut dir = PathBuf::from(".");
    let mut output = PathBuf::from("program.bin");
    let mut map_output = None;

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--entry" | "-e" => {
                i += 1;
                if i >= args.len() { die("--entry requires a module name"); }
                entry_module = args[i].clone();
            }
            "--dir" | "-d" => {
                i += 1;
                if i >= args.len() { die("--dir requires a path"); }
                dir = PathBuf::from(&args[i]);
            }
            "-o" => {
                i += 1;
                if i >= args.len() { die("-o requires a path"); }
                output = PathBuf::from(&args[i]);
            }
            "--map" => {
                i += 1;
                if i >= args.len() { die("--map requires a path"); }
                map_output = Some(PathBuf::from(&args[i]));
            }
            arg if arg.starts_with('-') => {
                die(&format!("unknown option '{}'", arg));
            }
            _ => {
                module_names.push(args[i].clone());
            }
        }
        i += 1;
    }

    if entry_module.is_empty() {
        die("--entry is required");
    }
    if module_names.is_empty() {
        die("no modules specified");
    }

    Args { entry_module, module_names, dir, output, map_output }
}

fn print_usage() {
    eprintln!("link24 -- COR24 module linker");
    eprintln!();
    eprintln!("Usage:");
    eprintln!("  link24 --entry <module> [--dir <path>] [-o <out.bin>] [--map <out.map>] <modules...>");
    eprintln!();
    eprintln!("Options:");
    eprintln!("  --entry, -e <name>   Entry module (placed first at address 0)");
    eprintln!("  --dir, -d <path>     Directory containing .bin and .meta files (default: .)");
    eprintln!("  -o <path>            Output binary (default: program.bin)");
    eprintln!("  --map <path>         Write symbol map file");
    eprintln!("  -h                   This help");
    eprintln!("  --help               Extended help");
}

fn print_help() {
    print_usage();
    eprintln!();
    eprintln!("=== Extended Help ===");
    eprintln!();
    eprintln!("link24 combines separately assembled COR24 modules into a single");
    eprintln!("binary. Each module consists of a .bin file (raw assembled binary)");
    eprintln!("and a .meta file (export/fixup metadata).");
    eprintln!();
    eprintln!("The entry module is placed at address 0. Remaining modules follow");
    eprintln!("in the order specified on the command line.");
    eprintln!();
    eprintln!("Cross-module references use FIXUP entries: the .meta file lists");
    eprintln!("every `la` instruction that references an external symbol. link24");
    eprintln!("patches the 3 address bytes of each `la` with the resolved address.");
    eprintln!();
    eprintln!(".meta file format:");
    eprintln!("  MODULE <name>");
    eprintln!("  EXPORT <symbol> <hex_offset>   ; label defined in this module");
    eprintln!("  FIXUP  <hex_offset> <symbol>   ; la instruction needing patching");
    eprintln!();
    eprintln!("Example:");
    eprintln!("  link24 --entry sno_main --dir build --map build/program.map \\");
    eprintln!("    sno_main sno_util sno_lex sno_exec -o build/program.bin");
    eprintln!();
    eprintln!("Then run:");
    eprintln!("  cor24-run --load-binary build/program.bin@0 --entry 0 --terminal");
    eprintln!();
    eprintln!("AI Agent Guidance:");
    eprintln!("  Each module must be assembled with --base-addr set to its assigned");
    eprintln!("  base address (two-pass assembly: first at base 0 for sizes, then");
    eprintln!("  reassembled with correct bases). The meta-gen tool automates .meta");
    eprintln!("  generation from .s/.lst files. See docs/linker-design.md.");
}

fn main() {
    let args = parse_args();

    if !args.module_names.contains(&args.entry_module) {
        die(&format!("entry module '{}' not in module list", args.entry_module));
    }

    // Build ordered list: entry first, then rest in specified order
    let mut ordered: Vec<&str> = vec![&args.entry_module];
    for name in &args.module_names {
        if name != &args.entry_module {
            ordered.push(name);
        }
    }

    let mut modules: Vec<LinkedModule> = Vec::new();
    for name in &ordered {
        let meta_path = args.dir.join(format!("{}.meta", name));
        let bin_path = args.dir.join(format!("{}.bin", name));

        let meta = parse_meta(&meta_path);
        let binary = fs::read(&bin_path)
            .unwrap_or_else(|e| die(&format!("cannot read {}: {}", bin_path.display(), e)));

        if meta.name != *name {
            die(&format!(
                "module name mismatch: file is '{}' but MODULE says '{}'",
                name, meta.name
            ));
        }

        modules.push(LinkedModule { meta, binary, base: 0 });
    }

    // Link
    let output = link(&mut modules);

    // Write combined binary
    fs::write(&args.output, &output)
        .unwrap_or_else(|e| die(&format!("cannot write {}: {}", args.output.display(), e)));
    eprintln!(
        "link24: {} bytes -> {}  ({} modules, {} exports, {} fixups)",
        output.len(),
        args.output.display(),
        modules.len(),
        modules.iter().map(|m| m.meta.exports.len()).sum::<usize>(),
        modules.iter().map(|m| m.meta.fixups.len()).sum::<usize>(),
    );

    // Write map if requested
    if let Some(map_path) = &args.map_output {
        write_map(map_path, &modules);
    }
}
