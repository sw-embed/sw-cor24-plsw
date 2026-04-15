// meta-gen -- Generate .meta files for link24
//
// Two-phase tool for preparing modules for separate compilation:
//
// Phase 1 (prep): Reads a .s file, identifies labels defined locally
//   and references to external symbols. Rewrites external `la rN,<sym>`
//   to `la rN,0` (assembleable placeholder). Writes modified .s and a
//   .syms file listing external references and their source lines.
//
// Phase 2 (emit): After assembly, reads the .lst file and .syms file
//   to produce the final .meta with byte-accurate EXPORT and FIXUP offsets.
//
// Usage:
//   meta-gen prep <input.s> -o <output.s> [--syms <output.syms>]
//   meta-gen emit <input.lst> --syms <input.syms> --module <name> -o <output.meta>

use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::Write;
use std::process;

fn die(msg: &str) -> ! {
    eprintln!("meta-gen: error: {}", msg);
    process::exit(1);
}

// --- Phase 1: prep ---

fn cmd_prep(input: &str, output: &str, syms_path: &str) {
    let content = fs::read_to_string(input)
        .unwrap_or_else(|e| die(&format!("cannot read {}: {}", input, e)));

    // Pass 1: collect all labels defined in this file
    let mut defined: HashSet<String> = HashSet::new();
    for line in content.lines() {
        let trimmed = line.trim();
        // Label definition: starts at column 0, ends with ':'
        if !trimmed.is_empty()
            && !trimmed.starts_with(' ')
            && !trimmed.starts_with('\t')
            && !trimmed.starts_with(';')
            && !trimmed.starts_with('.')
        {
            if let Some(label) = trimmed.strip_suffix(':') {
                defined.insert(label.to_string());
            }
        }
    }

    // Pass 2: find external references and rewrite
    let mut out_lines: Vec<String> = Vec::new();
    let mut extern_refs: Vec<(usize, String)> = Vec::new(); // (line_num, symbol)

    for (line_num, line) in content.lines().enumerate() {
        let trimmed = line.trim();

        // Match: la rN,<symbol> where <symbol> starts with _
        // Patterns: "la r0,_SYM", "la r2,_SYM", "la r1,_SYM"
        if let Some(la_match) = parse_la_external(trimmed, &defined) {
            extern_refs.push((line_num + 1, la_match.symbol.clone()));
            // Rewrite to la rN,0
            let new_line = line.replace(
                &format!(",{}", la_match.symbol),
                ",0",
            );
            out_lines.push(new_line);
        } else {
            out_lines.push(line.to_string());
        }
    }

    // Write modified .s
    let mut f = fs::File::create(output)
        .unwrap_or_else(|e| die(&format!("cannot create {}: {}", output, e)));
    for line in &out_lines {
        writeln!(f, "{}", line).unwrap();
    }

    // Write .syms (external references with source line numbers)
    let mut sf = fs::File::create(syms_path)
        .unwrap_or_else(|e| die(&format!("cannot create {}: {}", syms_path, e)));
    // Header: defined labels (for reference)
    writeln!(sf, "; Defined labels in this module").unwrap();
    let mut sorted_defined: Vec<&String> = defined.iter().collect();
    sorted_defined.sort();
    for label in &sorted_defined {
        writeln!(sf, "DEF {}", label).unwrap();
    }
    writeln!(sf, "; External references (source_line symbol)").unwrap();
    for (line_num, sym) in &extern_refs {
        writeln!(sf, "REF {} {}", line_num, sym).unwrap();
    }

    eprintln!(
        "meta-gen: prep {} -> {} ({} defined, {} external refs)",
        input, output, defined.len(), extern_refs.len()
    );
}

struct LaMatch {
    symbol: String,
}

fn parse_la_external(line: &str, defined: &HashSet<String>) -> Option<LaMatch> {
    // Match "la rN,_SYMBOL" where _SYMBOL is not defined locally.
    // Accepts both user externals (e.g. _UART_PUTS) and PL/SW runtime
    // helpers (e.g. __plsw_div) -- the second character may be an
    // underscore as well as a letter (issue #44).
    let line = line.trim();
    if !line.starts_with("la ") {
        return None;
    }
    let rest = line[3..].trim();
    // rest is "rN,_SYMBOL" or "rN,number" etc.
    let comma = rest.find(',')?;
    let target = rest[comma + 1..].trim();

    // Symbol names start with '_'; numeric immediates never do.
    if !target.starts_with('_') {
        return None;
    }
    // Require an identifier-continue char after the leading '_' so we
    // do not match stray underscores; accepts letters, digits, and '_'.
    match target.chars().nth(1) {
        Some(c) if c.is_ascii_alphanumeric() || c == '_' => {}
        _ => return None,
    }

    let symbol = target.to_string();
    if defined.contains(&symbol) {
        return None; // Local -- not external
    }

    Some(LaMatch { symbol })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn defined_set(labels: &[&str]) -> HashSet<String> {
        labels.iter().map(|s| s.to_string()).collect()
    }

    #[test]
    fn user_external_is_detected() {
        let d = defined_set(&[]);
        let m = parse_la_external("la r2,_UART_PUTS", &d).expect("should match");
        assert_eq!(m.symbol, "_UART_PUTS");
    }

    #[test]
    fn plsw_runtime_helper_is_detected() {
        // Regression test for issue #44: __plsw_div has '_' as the
        // second character and used to be skipped by the detector.
        let d = defined_set(&[]);
        let m = parse_la_external("la r2,__plsw_div", &d).expect("should match");
        assert_eq!(m.symbol, "__plsw_div");
    }

    #[test]
    fn locally_defined_symbol_is_not_external() {
        let d = defined_set(&["_ENTRY"]);
        assert!(parse_la_external("la r0,_ENTRY", &d).is_none());
    }

    #[test]
    fn numeric_immediate_is_not_matched() {
        let d = defined_set(&[]);
        assert!(parse_la_external("la r2,16711937", &d).is_none());
        assert!(parse_la_external("la r2,0", &d).is_none());
    }

    #[test]
    fn prep_rewrites_plsw_div_reference() {
        // End-to-end prep pass: feed a module that uses __plsw_div and
        // check the rewritten source + .syms REF entries.
        let tmp = std::env::temp_dir().join("meta_gen_test_issue44");
        let _ = fs::create_dir_all(&tmp);
        let input = tmp.join("mod.s");
        let output = tmp.join("mod_prep.s");
        let syms = tmp.join("mod.syms");

        fs::write(
            &input,
            "        .text\n\
             _CALLER:\n\
            \x20       la      r2,__plsw_div\n\
            \x20       jal     r1,(r2)\n",
        )
        .unwrap();

        cmd_prep(
            input.to_str().unwrap(),
            output.to_str().unwrap(),
            syms.to_str().unwrap(),
        );

        let prepped = fs::read_to_string(&output).unwrap();
        assert!(
            prepped.contains("la      r2,0"),
            "prep should rewrite la r2,__plsw_div to la r2,0; got:\n{}",
            prepped
        );
        assert!(!prepped.contains("__plsw_div"));

        let syms_text = fs::read_to_string(&syms).unwrap();
        assert!(
            syms_text.contains("REF ") && syms_text.contains("__plsw_div"),
            ".syms should list __plsw_div as a REF; got:\n{}",
            syms_text
        );
    }
}

// --- Phase 2: emit ---

fn cmd_emit(lst_path: &str, syms_path: &str, module_name: &str, output: &str) {
    let lst_content = fs::read_to_string(lst_path)
        .unwrap_or_else(|e| die(&format!("cannot read {}: {}", lst_path, e)));
    let syms_content = fs::read_to_string(syms_path)
        .unwrap_or_else(|e| die(&format!("cannot read {}: {}", syms_path, e)));

    // Parse .syms: collect defined labels and external references
    let mut defined_labels: HashSet<String> = HashSet::new();
    let mut ext_refs: Vec<(usize, String)> = Vec::new(); // (source_line, symbol)

    for line in syms_content.lines() {
        let line = line.trim();
        if line.starts_with(';') || line.is_empty() {
            continue;
        }
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() >= 2 && parts[0] == "DEF" {
            defined_labels.insert(parts[1].to_string());
        } else if parts.len() >= 3 && parts[0] == "REF" {
            let line_num: usize = parts[1].parse().unwrap_or(0);
            ext_refs.push((line_num, parts[2].to_string()));
        }
    }

    // Parse .lst: build label->offset map and find la instructions at offset 0
    // (these are the rewritten external refs)
    let mut label_offsets: HashMap<String, u32> = HashMap::new();
    let mut la_zero_offsets: Vec<u32> = Vec::new(); // byte offsets of "la rN,0" instructions

    for line in lst_content.lines() {
        let trimmed = line.trim();

        // Label definition line: just "labelname:" with no hex prefix
        if !trimmed.is_empty()
            && !trimmed.starts_with(|c: char| c.is_ascii_hexdigit())
            && trimmed.ends_with(':')
        {
            let label = &trimmed[..trimmed.len() - 1];
            // The NEXT line with a hex address gives the offset
            // Actually, labels in .lst appear on their own line, then the
            // next instruction line has the offset. We handle this below.
            // For now, record that we saw a label -- we'll get its offset
            // from the next instruction line.
            label_offsets.insert(label.to_string(), 0xFFFFFF); // placeholder
            continue;
        }

        // Instruction line: "XXXX: YY ..." where XXXX is hex offset
        if let Some(colon_pos) = trimmed.find(':') {
            let hex = &trimmed[..colon_pos];
            if let Ok(offset) = u32::from_str_radix(hex.trim(), 16) {
                // Update any pending label to this offset
                for (_, v) in label_offsets.iter_mut() {
                    if *v == 0xFFFFFF {
                        *v = offset;
                    }
                }

                // Check if this is an "la rN,0" (rewritten external ref)
                // la opcodes: r0=0x29, r1=0x2A, r2=0x2B
                // "la rN,0" assembles as: [opcode] 00 00 00
                let after_colon = trimmed[colon_pos + 1..].trim();
                // Format: "29 00 00 00    la      r0,0"
                if after_colon.len() >= 11 {
                    let bytes_part = &after_colon[..11]; // "XX 00 00 00"
                    if (bytes_part.starts_with("29") ||
                        bytes_part.starts_with("2A") ||
                        bytes_part.starts_with("2a") ||
                        bytes_part.starts_with("2B") ||
                        bytes_part.starts_with("2b"))
                        && bytes_part.ends_with("00 00 00")
                    {
                        // Verify it's "la rN,0" in the disassembly part
                        if after_colon.contains("la ") && after_colon.contains(",0") {
                            la_zero_offsets.push(offset);
                        }
                    }
                }
            }
        }
    }

    // Match la-zero instructions to external references (by order)
    // The prep phase replaced external refs in source order,
    // and they appear in the same order in the listing.
    if la_zero_offsets.len() != ext_refs.len() {
        eprintln!(
            "meta-gen: warning: {} la-zero instructions but {} external refs in .syms",
            la_zero_offsets.len(), ext_refs.len()
        );
        eprintln!("  la-zero offsets: {:?}", la_zero_offsets.iter().map(|o| format!("0x{:04X}", o)).collect::<Vec<_>>());
        if la_zero_offsets.len() < ext_refs.len() {
            die("fewer la-zero instructions than external refs -- assembly may have failed");
        }
    }

    // Write .meta
    let mut f = fs::File::create(output)
        .unwrap_or_else(|e| die(&format!("cannot create {}: {}", output, e)));

    writeln!(f, "; {}.meta -- generated by meta-gen", module_name).unwrap();
    writeln!(f, "MODULE {}", module_name).unwrap();
    writeln!(f).unwrap();

    // Exports: labels that start with _ AND are in the .syms DEF list.
    // Labels in the .lst but not in DEF are internal (e.g. __plsw_div
    // referenced but not defined in LIBRARY modules) (#41).
    let mut exports: Vec<(&String, u32)> = label_offsets.iter()
        .filter(|(name, offset)| {
            name.starts_with('_')
                && **offset != 0xFFFFFF
                && defined_labels.contains(*name)
        })
        .map(|(name, offset)| (name, *offset))
        .collect();
    exports.sort_by_key(|e| e.1);

    for (sym, offset) in &exports {
        writeln!(f, "EXPORT {} 0x{:04X}", sym, offset).unwrap();
    }

    if !ext_refs.is_empty() {
        writeln!(f).unwrap();
    }

    // Fixups: match external refs to la-zero offsets
    let fixup_count = ext_refs.len().min(la_zero_offsets.len());
    for i in 0..fixup_count {
        writeln!(f, "FIXUP 0x{:04X} {}", la_zero_offsets[i], ext_refs[i].1).unwrap();
    }

    eprintln!(
        "meta-gen: emit {} -> {} ({} exports, {} fixups)",
        lst_path, output, exports.len(), fixup_count
    );
}

// --- CLI ---

fn main() {
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 2 || args.iter().any(|a| a == "-h" || a == "--help") {
        eprintln!("meta-gen -- Generate .meta files for link24");
        eprintln!();
        eprintln!("Usage:");
        eprintln!("  meta-gen prep <input.s> -o <output.s> [--syms <output.syms>]");
        eprintln!("  meta-gen emit <input.lst> --syms <input.syms> --module <name> -o <output.meta>");
        eprintln!();
        eprintln!("Phase 1 (prep): Rewrites external la references to la rN,0");
        eprintln!("  placeholders. Writes .syms file listing external symbols.");
        eprintln!();
        eprintln!("Phase 2 (emit): Reads .lst + .syms to produce .meta with");
        eprintln!("  byte-accurate EXPORT and FIXUP entries for link24.");
        process::exit(0);
    }

    match args[1].as_str() {
        "prep" => {
            let mut input = String::new();
            let mut output = String::new();
            let mut syms = String::new();

            let mut i = 2;
            while i < args.len() {
                match args[i].as_str() {
                    "-o" => { i += 1; output = args.get(i).cloned().unwrap_or_default(); }
                    "--syms" => { i += 1; syms = args.get(i).cloned().unwrap_or_default(); }
                    _ if input.is_empty() => { input = args[i].clone(); }
                    _ => die(&format!("unexpected argument '{}'", args[i])),
                }
                i += 1;
            }

            if input.is_empty() { die("prep: input .s file required"); }
            if output.is_empty() { die("prep: -o <output.s> required"); }
            if syms.is_empty() {
                syms = output.replace(".s", ".syms");
            }

            cmd_prep(&input, &output, &syms);
        }
        "emit" => {
            let mut lst = String::new();
            let mut syms = String::new();
            let mut module_name = String::new();
            let mut output = String::new();

            let mut i = 2;
            while i < args.len() {
                match args[i].as_str() {
                    "--syms" => { i += 1; syms = args.get(i).cloned().unwrap_or_default(); }
                    "--module" => { i += 1; module_name = args.get(i).cloned().unwrap_or_default(); }
                    "-o" => { i += 1; output = args.get(i).cloned().unwrap_or_default(); }
                    _ if lst.is_empty() => { lst = args[i].clone(); }
                    _ => die(&format!("unexpected argument '{}'", args[i])),
                }
                i += 1;
            }

            if lst.is_empty() { die("emit: input .lst file required"); }
            if syms.is_empty() { die("emit: --syms required"); }
            if module_name.is_empty() { die("emit: --module required"); }
            if output.is_empty() { die("emit: -o <output.meta> required"); }

            cmd_emit(&lst, &syms, &module_name, &output);
        }
        cmd => die(&format!("unknown command '{}' (use 'prep' or 'emit')", cmd)),
    }
}
