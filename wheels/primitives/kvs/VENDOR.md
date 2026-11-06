# Upstream

- **Project**: FlashDB
- **Author**: armink
- **URL**: https://github.com/armink/FlashDB
- **Version**: 2.1.0 (user's production-tested copy)
- **License**: Apache-2.0
- **Vendored**: 2026-04-28

## What was vendored

Core KVDB + TSDB source only:

- `inc/flashdb.h` — public API
- `inc/fdb_def.h` — types, macros, error codes
- `inc/fdb_low_lvl.h` — internal low-level API
- `src/fdb.c` — core init, flash ops
- `src/fdb_kvdb.c` — key-value database
- `src/fdb_tsdb.c` — time series database
- `src/fdb_utils.c` — utilities (CRC32, etc.)

Excluded:
- `inc/fdb_cfg.h` — RT-Thread specific (each project provides its own)
- `src/fdb_cmd.c` — RT-Thread FinSH shell commands
- `src/fdb_file.c` — file-mode backend (we use FAL mode)
- `docs/`, `samples/`, `tests/` — upstream docs and tests

## Source

Vendored from user's production copy (previously used in an RT-Thread
project). Core source files are unmodified from FlashDB v2.1.0, except for
these changes to `src/fdb_kvdb.c`, each marked `Local change` in the file:

- `find_next_kv_addr` casts to `uint32_t` before a shift by 24, which UBSan
  flagged as overflowing `int`.
- `read_kv` treats a record with `name_len` above `FDB_KV_NAME_MAX` as bad
  even when its CRC is valid. Upstream reads the whole name into the fixed
  `kv->name` first and overruns it, so a build with a smaller limit than
  the one that wrote the flash crashed in `fdb_kvdb_init`.
- `create_kv_blob` refuses a key of exactly `FDB_KV_NAME_MAX` characters, as
  the comment in `fdb_def.h` asks. Upstream accepts it, but `read_kv` replaces
  its last character with the NUL, so the key cannot be found after a reboot.

And these to `src/fdb_tsdb.c`, marked the same way:

- `read_sector_info` clears `status` when the magic word is wrong. Without
  that a sector with a bad header inherits the status of the sector read
  before it, and the format callbacks skip one that inherits EMPTY, so a
  sector left blank by a power cut stayed blank through init and
  `fdb_tsl_clean`, and the next boot formatted away the logs written since.
- `fdb_tsl_iter`, `fdb_tsl_iter_reverse` and `fdb_tsl_iter_by_time` return
  after their "isn't initialize OK" message, as the other TSDB calls do. They
  carried on, and after a failed init the sector size is 0, so the traversal
  never ended and held the database lock.

And this to `src/fdb.c`, marked the same way:

- `_fdb_db_path` returns an empty string when `storage.part` is NULL, which it
  is after a failed partition lookup. Every message on that path prints the
  path, starting with the failure message of `fdb_tsdb_init` itself.

The RT-Thread-specific `fdb_cfg.h` is replaced per-project.

## Update procedure

1. Download FlashDB release from GitHub
2. Copy `inc/flashdb.h`, `inc/fdb_def.h`, `inc/fdb_low_lvl.h` to `vendor/inc/`
3. Copy `src/fdb.c`, `src/fdb_kvdb.c`, `src/fdb_tsdb.c`, `src/fdb_utils.c` to `vendor/src/`
4. Run tests: `cd test && cmake -GNinja -S . -B build && cmake --build build && ./build/test_kvs`
5. Reapply the `fdb_kvdb.c`, `fdb_tsdb.c` and `fdb.c` changes above unless upstream has fixed them
6. Update version and date in this file
