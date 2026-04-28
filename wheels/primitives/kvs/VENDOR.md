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
project). Core source files are unmodified from FlashDB v2.1.0.
The RT-Thread-specific `fdb_cfg.h` is replaced per-project.

## Update procedure

1. Download FlashDB release from GitHub
2. Copy `inc/flashdb.h`, `inc/fdb_def.h`, `inc/fdb_low_lvl.h` to `vendor/inc/`
3. Copy `src/fdb.c`, `src/fdb_kvdb.c`, `src/fdb_tsdb.c`, `src/fdb_utils.c` to `vendor/src/`
4. Run tests: `cd test && cmake -GNinja -S . -B build && cmake --build build && ./build/test_kvs`
5. Update version and date in this file
