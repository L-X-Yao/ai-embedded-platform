# fal — Flash Abstraction Layer

**Status: candidate**

Flash partition management layer. Registers flash devices, defines partition
table (name/offset/size), provides unified read/write/erase API on partitions
with boundary protection.

## Origin

Self-written (ADR-010). API-compatible with armink/FAL for downstream consumers
(kvs/FlashDB).

## Why self-written?

The original implementation vendored armink/FAL from RT-Thread. That upstream
carries RT-Thread headers, a logging subsystem, and build infrastructure that
conflict with our zero-dependency wheel contract. The self-written replacement
preserves the external API (`fal_partition_read/write/erase`, device and
partition structs) so that FlashDB/kvs consumers compile unchanged, while
removing all RT-Thread coupling. Total implementation: ~150 lines of pure C99.

## Dependencies

- None (interface dependency: flash device ops provided by project at compile time)

## API

Consumers `#include <fal.h>`.

```c
/* Project provides fal_cfg.h with device table + partition table */

int n = fal_init();   /* returns partition count on success */

const struct fal_partition *p = fal_partition_find("config");
fal_partition_read(p, 0, buf, sizeof(buf));
fal_partition_write(p, 0, data, len);
fal_partition_erase(p, 0, 4096);
fal_partition_erase_all(p);

const struct fal_flash_dev *d = fal_flash_device_find("onchip");
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `FAL_DEV_NAME_MAX` | 24 | Max device/partition name length |
| `FAL_PRINTF` | (none) | Define to enable `fal_show_part_table()` output |

## Supported platforms

- [x] Host (sim, RAM-backed flash mock)
- [ ] GD32F405 (flybox) — FAL used via kvs in project

## Known issues

None.

## How to test

```bash
cd wheels/primitives/fal/test
cmake -GNinja -S . -B build && cmake --build build && ctest --test-dir build
```

## Test status

- Unit tests (sim): PASS (20 C tests + 6 null parameter tests + 5 property tests)
- HIL: N/A

## API convention deviations

- `fal_init()` returns the partition count (>= 0) on success rather than 0.
  This matches armink/FAL's contract, which FlashDB and kvs rely on. The
  function is documented as "returns partition count on success, a negative
  `fal_err_t` on failure". Negative return still signals error per
  api-conventions §2.1(b).

## Porting (per-project fal_cfg.h)

Each project must provide `fal_cfg.h` on the include path:

```c
extern const struct fal_flash_dev onchip_flash;
#define FAL_FLASH_DEV_TABLE { &onchip_flash }

#define FAL_PART_TABLE \
{ \
    {"bootloader", "onchip", 0,       0x10000}, \
    {"app",        "onchip", 0x10000, 0x70000}, \
    {"kvdb",       "onchip", 0x80000, 0x10000}, \
}
```

And implement the flash device:

```c
const struct fal_flash_dev onchip_flash = {
    .name     = "onchip",
    .addr     = 0x08000000,
    .len      = 1024 * 1024,
    .blk_size = 4096,
    .ops      = { .init = ..., .read = ..., .write = ..., .erase = ... },
    .write_gran = 8,
};
```

All ops (`.read`, `.write`, `.erase`) must be non-NULL. `.init` is optional
(NULL is accepted and silently skipped during `fal_init()`).

`fal_init()` must succeed before any partition call: until then they return
`FAL_ERR_NOT_READY`. It returns `FAL_ERR_PART_TABLE`, before touching any
device, if a partition does not fit inside its device, so `.len` has to be
right. It returns `FAL_ERR_HAL` if an `.init` fails, and can then be retried.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Self-written replacing vendored FAL. API-compatible with FAL for kvs interop. Static partition table only. No RT-Thread dependencies, no dynamic allocation.

## Responsible human

@L-X-Yao
