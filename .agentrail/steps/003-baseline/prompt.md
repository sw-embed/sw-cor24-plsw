Capture post-fix baseline measurements, write the rebuild-required
CHANGES.md entry, and close the saga.

Steps:

1. `just clean && just build-lgo && just build-test-lgo`.

2. Sizes + SHAs:
   - `wc -c build/plsw.lgo` + `sha256sum build/plsw.lgo`.
   - `wc -c build/plsw_test.lgo` + `sha256sum build/plsw_test.lgo`.

3. Compare against three earlier reference points:
   - Pre-Phase-3 (rolled-back production): **1,657,430** bytes
     (SHA `712fb0be…` per CHANGES.md).
   - Broken Phase-3 (current main, not installed):
     **872,174** bytes (SHA `38774800…`).
   - Today's fixed build (this step's measurement).

4. Update `docs/shrink-lgo-size.md`:
   - Add a "Status (2026-05-12, part 2)" section noting:
     - The capacity regression discovered in mike's brief
       2026-05-12 17:01.
     - The new CHUNK_MAX (and/or CHUNK_SIZE) values from
       step 2.
     - The new measured peak from step 1.
     - The new headroom ratio (peak vs. capacity).
     - The new SRC_BUF_SIZE (192 KiB; interim per the
       enlarge-src-buf brief).
   - In Phase 2b's DONE block: replace the now-stale "5.4×
     headroom over 406-node fixture peak" wording with the
     new peak + headroom. Keep the saga DONE marker.
   - In the buffer-sizes table at the top: bump `src_buf` row
     from 65,536 to 196,608.

5. Update `CHANGES.md`:
   - New entry at the top dated 2026-05-12.
   - **Rebuild signal: REQUIRED.** The prior 2026-05-12 entry
     (872-KB binary) **supersedes** in the sense that it
     describes the version that was rolled back; this new
     entry is what mike should install instead.
   - Record:
     - Both bumps (chunk capacity, SRC_BUF).
     - New plsw.lgo SHA + size + delta vs both the rolled-back
       and the broken-Phase-3 baselines.
     - Real-workload unblock note (`sno_lex.plsw` compiles
       end-to-end).
     - reg-rs result (16/16 green after fixture add).
     - Rebuild command (same shape as prior entries).

6. Optional: run `just test` once more for the CHANGES entry's
   verification.

7. Commit on `feat/capacity-and-test`. Include the doc
   updates and `.agentrail/` bookkeeping.

8. `agentrail complete --done --summary "..." --reward 1
   --actions "..."`. **--done** closes the saga.

9. `dg-mark-pr` and STOP.

## Out of scope

* Promoting the .lgo to install (mike's job per the brief).
* Adding any other test fixtures beyond the one in step 2.
* Next saga's choices (buffer-to-chunks, static-lint,
  shrink-lgo-verify, or your own ideas) -- decided after
  this lands.