Saga plan step 2: Vendor sw-cx24 (the C cross-compiler).

Per docs/vendor-plan.md and the saga plan:
- Pin a stable upstream commit of the C compiler (currently at ~/github/sw-vibe-coding/tc24r)
- Run scripts/vendor-fetch.sh --record sw-cx24 to capture sha256
- Copy includes/ and docs/ from the upstream tree
- Verify the vendored binary builds PL/SW byte-identical to the current tc24r-built version
- May involve an upstream rename of the C compiler repo (tc24r -> sw-cx24)

End state: 'just build' uses vendor/sw-cx24/<version>/bin/sw-cx24 (or tc24r) and produces byte-identical build/plsw.s.