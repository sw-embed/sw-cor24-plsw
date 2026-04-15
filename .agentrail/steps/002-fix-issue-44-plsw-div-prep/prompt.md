Fix GitHub issue #44: meta-gen prep doesn't zero out __plsw_div runtime helper, causing silent assemble failures.

The PL/SW meta-gen prep tool rewrites external symbol references to 'la r2, 0' so modules can Pass-1 assemble. It handles user-facing externals correctly but does NOT handle the PL/SW runtime helper __plsw_div that the compiler emits for integer division.

Preferred fix (from issue): Treat __plsw_div (and any __plsw_* helper) as an implicit external in meta-gen prep. Whenever prep sees 'la r2, __plsw_div', rewrite to 'la r2, 0' and emit a FIXUP entry in the .meta file pointing at the helper symbol.

Acceptance:
- meta-gen prep recognizes __plsw_* helper symbols as implicit externals
- Reproducer in issue #44 no longer fails: prep.s assembles cleanly without 'Undefined label __plsw_div'
- Add test fixture covering a module that uses integer division
- Close issue #44 on commit (use 'Closes #44' in commit message)

See https://github.com/sw-embed/sw-cor24-prolog/blob/main/docs/stack-corruption.md for the downstream impact context.