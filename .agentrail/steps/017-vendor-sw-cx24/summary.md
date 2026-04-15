BLOCKED: Step requires user decisions before any vendoring can happen:

1. Repo identity for sw-cx24. The vendor plan says repo URL should be 'git@github.com:sw-embed/sw-cx24.git', but no such repo exists. Three live alternatives:
   a. Vendor from tc24r (~/github/sw-vibe-coding/tc24r) as-is, with version.json.repo pointing at git@github.com:sw-vibe-coding/tc24r.git. Pragmatic, but the name 'sw-cx24' diverges from upstream forever.
   b. Wait for ~/github/sw-embed/sw-cor24-tinyc to be populated -- it currently has only README + 'placeholder' commits. The tc24r README says active dev moves there, but it hasn't.
   c. Create a new sw-embed/sw-cx24 repo, push tc24r's content into it under the new name. Most aligned with the plan, requires user to create the github repo and decide whether to keep history.

2. Build command. tc24r has no root Cargo.toml; it's a multi-crate project (~50 components/) built via scripts/build-all.sh. Need to pick which produced binary becomes vendor/sw-cx24/v0.1.0/bin/sw-cx24.

3. Pinned commit. tc24r HEAD is f227a70 ('docs: stabilized redirect notice'). Good candidate or pin to an earlier known-good commit?

4. Includes provenance. tc24r/include/ has cor24.h, stdbool.h, stdio.h, stdlib.h, string.h, test.h. Copy verbatim into vendor/sw-cx24/v0.1.0/includes/?

These are all 'check with the user before proceeding' items per CLAUDE.md (rename a github repo / populate a stabilized fork / decide canonical naming -- all hard-to-reverse, affect shared systems). Need user direction; should not pick on agent's behalf.