# Upstream

- **Project**: FAL (Flash Abstraction Layer)
- **Author**: armink (RT-Thread Development Team)
- **URL**: https://github.com/armink/FlashDB/tree/master/port/fal
- **Standalone (archived)**: https://github.com/RT-Thread-packages/fal
- **Version**: 0.5.99
- **License**: Apache-2.0
- **Vendored**: 2026-04-28

## What was vendored

Core files only (no RT-Thread block device wrapper):

- `inc/fal.h` — public API (RT-Thread device functions stripped)
- `inc/fal_def.h` — types, macros, bare-metal fallbacks
- `src/fal.c` — init
- `src/fal_flash.c` — device table, init, find
- `src/fal_partition.c` — partition table (static config mode only)

Excluded:
- `src/fal_rtt.c` — RT-Thread block device layer (not needed on bare-metal/VSF)
- Dynamic partition table loading (requires flash scan at boot, not needed when using `FAL_PART_HAS_TABLE_CFG`)

## Modifications from upstream

1. `fal.h` — removed RT-Thread device API declarations (`fal_blk_device_create`, `fal_mtd_nor_device_create`, `fal_char_device_create`) to avoid incomplete type warnings without `rtthread.h`. Added `fal_init_check()` declaration.
2. `fal_partition.c` — replaced dynamic partition table loading (`#ifndef FAL_PART_HAS_TABLE_CFG` path) with `#error` since we only use static config mode.

## Update procedure

1. Fetch upstream FAL from FlashDB repo (`port/fal/`)
2. Copy `inc/fal_def.h` and `src/fal.c`, `src/fal_flash.c` as-is
3. Apply the two modifications listed above to `fal.h` and `fal_partition.c`
4. Run tests: `cd test && cmake -GNinja -S . -B build && cmake --build build && ./build/test_fal`
5. Update version and date in this file
