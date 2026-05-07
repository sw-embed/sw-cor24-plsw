Reconcile the 12 orphan commits and untracked files reported by 'agentrail audit' at the start of step 002, so subsequent audits come up clean.

Scope:
1. Run 'agentrail audit --emit-commands' to scaffold retroactive 'agentrail add' lines for the 12 orphan commits (from 3315953d up through e8e2a40).
2. Review the emitted commands -- sanity check slugs/prompts against 'git log' subjects -- and execute them.
3. Inspect the 11 untracked working-tree entries (components/linker/{Cargo.toml,Cargo.lock,main.rs,tests/demo-fixup.sh,tests/fixtures-fixup/}, docs/linker-design.md, docs/macro-prologue.md, linker-research.txt, plus the post-complete agentrail artifacts). For each, decide: commit in this step, add to .gitignore, or leave for a later step with a brief rationale.
   - components/linker/target/ -> .gitignore (build artifact)
   - everything else -> commit if it belongs in the repo, note otherwise.
4. Commit the working-tree reconciliation as one focused bundle (scope 'chore(agentrail)' or 'chore(repo)').
5. Run 'agentrail audit' at the end and confirm the orphan commit and uncommitted-entry counts are both 0 (or document any that are intentionally deferred).

Do NOT add any new features or fixes in this step. Pure bookkeeping.