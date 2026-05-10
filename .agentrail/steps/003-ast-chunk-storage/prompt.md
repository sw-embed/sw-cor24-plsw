Switch the AST accessor macros (added in `ast-accessors`) to
chunk-backed storage. The static `nd_*_arr[NODE_POOL_MAX]` arrays
go away; AST nodes live in chunks drawn from `chunk_alloc()`.

Steps:

1. Define the per-chunk node block layout in `src/ast.h`:
   ```c
   #define AST_NODES_PER_CHUNK 136   /* 4096 / (10 * 3) = 136 */

   struct ast_block {
       int    kind  [AST_NODES_PER_CHUNK];
       int    type  [AST_NODES_PER_CHUNK];
       int    stor  [AST_NODES_PER_CHUNK];
       int    level [AST_NODES_PER_CHUNK];
       int    left  [AST_NODES_PER_CHUNK];
       int    right [AST_NODES_PER_CHUNK];
       int    next  [AST_NODES_PER_CHUNK];
       int    ival  [AST_NODES_PER_CHUNK];
       int    line  [AST_NODES_PER_CHUNK];
       char  *name  [AST_NODES_PER_CHUNK];
   };
   ```
   Each `ast_block` is sizeof(int)*9 + sizeof(char*) all times
   AST_NODES_PER_CHUNK = 30 * 136 = 4080 bytes -- under the
   4096 chunk size with 16 bytes of slack.

   Verify the math against the actual layout tc24r picks
   (`grep ast_block build/plsw.s`); adjust
   AST_NODES_PER_CHUNK down if alignment pushes the struct
   above 4096.

2. AST chunk table:
   ```c
   #define AST_CHUNKS_MAX CHUNK_MAX  /* worst case: AST owns all chunks */
   struct ast_block *ast_chunks[AST_CHUNKS_MAX];
   int ast_chunk_count;
   ```

3. Switch the accessor macros from `nd_kind_arr[i]` to chunk
   form:
   ```c
   #define nd_kind(i)  (ast_chunks[(i) / AST_NODES_PER_CHUNK]->kind[(i) % AST_NODES_PER_CHUNK])
   ```
   ...and similarly for the other 9 fields. The macro layer
   already added in `ast-accessors` means callers stay
   unchanged.

4. Update `ast_init()`:
   ```c
   void ast_init(void) {
       int i;
       i = 0;
       while (i < ast_chunk_count) {
           chunk_free((char *)ast_chunks[i]);
           ast_chunks[i] = 0;
           i = i + 1;
       }
       ast_chunk_count = 0;
       nd_count = 0;
       ast_pool_exhausted = 0;
   }
   ```

5. Update `nd_alloc()`:
   ```c
   int nd_alloc(int kind) {
       int chunk_idx = nd_count / AST_NODES_PER_CHUNK;
       /* Need a new chunk? */
       if (chunk_idx >= ast_chunk_count) {
           if (ast_chunk_count >= AST_CHUNKS_MAX) {
               /* exhaustion -- same error path as before, but
                  message updated */
               ...
               return NODE_NULL;
           }
           char *p = chunk_alloc();
           if (!p) {
               /* same exhaustion path */
               return NODE_NULL;
           }
           ast_chunks[ast_chunk_count] = (struct ast_block *)p;
           ast_chunk_count = ast_chunk_count + 1;
       }
       int i = nd_count;
       nd_count = nd_count + 1;
       /* zero-fill the slot */
       nd_kind(i)  = kind;
       nd_type(i)  = TYPE_NONE;
       nd_stor(i)  = STOR_AUTO;
       nd_level(i) = 0;
       nd_left(i)  = NODE_NULL;
       nd_right(i) = NODE_NULL;
       nd_next(i)  = NODE_NULL;
       nd_ival(i)  = 0;
       nd_line(i)  = lex_line;
       nd_name(i)  = 0;
       return i;
   }
   ```

6. Remove the static `nd_*_arr[NODE_POOL_MAX]` declarations.
   Keep `NODE_POOL_MAX` as a deprecated comment for one saga
   in case someone greps for it; the *real* cap is now
   `AST_CHUNKS_MAX * AST_NODES_PER_CHUNK`.

7. Build + test:
   - `just clean && just build-lgo` -- production succeeds.
   - `grep '^_nd_kind_arr:' build/plsw.s` -- must be empty
     (the static arrays are gone).
   - `grep '^_chunk_storage:' build/plsw.s` -- still present.
   - `wc -c build/plsw.lgo` -- should drop from ~1.66 MB to
     ~1.36-1.4 MB. Capture the number for step 4.
   - `just build-test-lgo` -- succeeds.
   - `just test` -- reg-rs 15/15 green.
   - Run plsw_test.lgo suite #37 (chunk tests) -- 0 errors.
   - Run plsw_test.lgo suite #4 (AST tests) and a parser/
     codegen suite to confirm AST behavior is unchanged.

8. Test ast_init() reset hygiene: in REPL mode, compile two
   programs back-to-back. Verify chunks are released between
   compiles (chunk_used() should return to 0 after each
   ast_init()).

9. Commit on `feat/ast-chunk-storage`. Include `.agentrail/`.
   Stop at commit.

10. `agentrail complete --summary "..." --reward 1
    --actions "..."`. Then `dg-mark-pr` and STOP.

## Risks

* Division by 136 is not power-of-two: if performance regresses
  badly on test runtimes, consider rounding AST_NODES_PER_CHUNK
  down to 128 (waste 8 slots per chunk = 2.4 KB total over 16
  chunks).
* Cache the (chunk_idx, slot) decomposition at the top of any
  hot helper that touches the same node many times in a row,
  if profiling shows it matters.

## Out of scope

* Removing NODE_POOL_MAX entirely (gradual deprecation).
* Migrating other pools (next saga: buffer-to-chunks).
