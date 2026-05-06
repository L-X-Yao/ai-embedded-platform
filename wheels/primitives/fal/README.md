# fal — Flash Abstraction Layer

**Status: candidate**

Flash partition management layer. Registers flash devices, defines partition
table (name/offset/size), provides unified read/write/erase API on partitions
with boundary protection.

## Origin

Self-written. API-compatible with armink's FAL for downstream consumers (kvs/FlashDB).

## Dependencies

- None (interface dependency: flash device ops provided by project at compile time)

## API

Consumers `#include <fal.h>`.

```c
/* Project provides fal_cfg.h with device table + partition table */

fal_init();

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

## How to test

```bash
cd wheels/primitives/fal/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_fal
```

## Test status

- Unit tests (sim): PASS (13 C tests + 5 property tests)
- HIL: N/A

## API review

- **Date**: 2026-05-06
- **Key decisions**: Self-written replacing vendored FAL. API-compatible with FAL for kvs interop. Static partition table only. No RT-Thread dependencies, no dynamic allocation.

## Responsible human

@user

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
    .len      = 512 * 1024,
    .blk_size = 4096,
    .ops      = { .init = ..., .read = ..., .write = ..., .erase = ... },
    .write_gran = 8,
};
```
