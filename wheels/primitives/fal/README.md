# fal — Flash Abstraction Layer

**Status: candidate**

Flash partition management layer. Registers flash devices, defines partition
table (name/offset/size), provides unified read/write/erase API on partitions
with boundary protection. Vendored from armink's FAL (v0.5.99).

## Origin

Vendor: see VENDOR.md.

## Why this upstream?

- FAL is the de facto flash abstraction in Chinese embedded (RT-Thread ecosystem)
- Designed as FlashDB's lower layer — our kvs wheel will use it
- 668 lines of pure C, zero RTOS dependency on bare-metal
- Static partition table (compile-time), no malloc in normal path

Evaluated alternatives: Zephyr NVS (not extractable), ESP NVS (ESP32 only).
See ADR-010.

## Dependencies

- None (interface dependency: flash device ops provided by project at compile time)

## API

FAL's upstream API is used directly (no wrapper). Consumers `#include <fal.h>`.

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
| `FAL_PART_HAS_TABLE_CFG` | (must define) | Use static partition table from fal_cfg.h |
| `FAL_DEV_NAME_MAX` | 24 | Max device/partition name length |
| `FAL_DEBUG` | 0 | Enable debug logging |
| `FAL_PRINTF` | printf | Override print function |

## Supported platforms

- [x] Host (sim, RAM-backed flash mock) — passed 2026-04-28
- [ ] GD32F405 (flybox) — FAL used via kvs in project, wheel not separately HIL tested

## Known issues

- Dynamic partition table loading (scan flash at boot) removed — only static config mode supported
- RT-Thread block device layer excluded — not applicable outside RT-Thread

## How to test

```bash
cd wheels/primitives/fal/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_fal
```

27 assertions across 13 test cases: init, partition find (existing/missing), flash device find, partition table query, write-read round-trip, offset write-read, erase, erase-all, boundary checks (read/write/erase out of bounds), partition isolation, flash write semantics (AND-only), show_part_table.

## Test status

- Unit tests (sim): PASS (13 C tests, 27 assertions)
- HIL: N/A

## API convention deviations

- **No `fal_` wrapper**: Uses upstream FAL API directly (`fal_partition_read` not `fal_wheel_read`). Wrapping a well-known API adds confusion for anyone familiar with FAL.

## API review

- **Date**: 2026-04-28
- **Key decisions**: Vendor FAL as-is, no wrapper (upstream API is already clean). Static partition table only. FAL is independently useful (OTA, bootloader, crash dump, not just FlashDB). First vendor wheel per ADR-010.

## Responsible human

@user

## Porting (per-project fal_cfg.h)

Each project must provide `fal_cfg.h` on the include path:

```c
#define FAL_PART_HAS_TABLE_CFG

extern const struct fal_flash_dev onchip_flash;
#define FAL_FLASH_DEV_TABLE { &onchip_flash }

#define FAL_PART_TABLE \
{ \
    {FAL_PART_MAGIC_WORD, "bootloader", "onchip", 0,      0x10000, 0}, \
    {FAL_PART_MAGIC_WORD, "app",        "onchip", 0x10000, 0x70000, 0}, \
    {FAL_PART_MAGIC_WORD, "kvdb",       "onchip", 0x80000, 0x10000, 0}, \
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
