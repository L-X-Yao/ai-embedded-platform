# ota_ab — A/B firmware slot manager with rollback

**Status: candidate**
**Layer: L2 (protocols)**

Platform-agnostic A/B firmware slot management for OTA upgrades. Handles slot selection,
parameter persistence with CRC32 protection, and automatic rollback after N failed boots.
Flash operations injected via HAL callbacks — no hardware dependency.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`ota`, `firmware`, `bootloader`, `a_b_slot`, `rollback` — zero results.

VSF has no OTA or firmware management capability.

## Origin

Extracted from `projects/flybox/src/app/ota_task.c` + `projects/flybox/src/bsp/internal_flash.c`.
Decoupled: HTTP download, modem interaction, FreeRTOS task scheduling stay in the project;
pure slot management logic moves to this wheel.

## Protocol (A/B boot state machine)

```
IDLE ──[mark_pending]──→ PENDING ──[boot_event]──→ SUCCESS (trial)
                                                       │
                                          [confirm]────┘──→ IDLE (new active)
                                          [N boots]────┘──→ IDLE (rollback to old)
```

## API

```c
ota_ab_t m;
ota_ab_layout_t layout = {
    .slot_a_addr   = 0x08010000,
    .slot_b_addr   = 0x08080000,
    .slot_max_size = 0x70000,
    .params_addr   = 0x080E0000,
    .params_size   = 4096,
    .max_boot_count = 3,
};
static bool my_erase(void *ctx, uint32_t addr, uint32_t size) { (void)ctx; /* ... */ return true; }
static bool my_write(void *ctx, uint32_t addr, const uint8_t *data, uint32_t len) { (void)ctx; /* ... */ return true; }
static bool my_read(void *ctx, uint32_t addr, uint8_t *buf, uint32_t len) { (void)ctx; /* ... */ return true; }
const ota_ab_flash_t flash_hal = { .erase = my_erase, .write = my_write, .read = my_read, .ctx = NULL };
ota_ab_init(&m, &flash_hal, &layout);

/* App: after downloading + CRC-verifying new firmware */
ota_ab_mark_pending(&m, fw_size, fw_crc);
reboot();

/* Bootloader: on every boot */
ota_ab_boot_event(&m);
jump_to(ota_ab_slot_addr(&m, ota_ab_active_slot(&m)));

/* App: after confirming new firmware works */
ota_ab_confirm(&m);
```

## Dependencies

None. Pure C99 with injected flash HAL callbacks.

## Supported platforms

- [x] Host (sim) — passed 2026-04-28
- [ ] GD32F405 (flybox) — original project code verified, wheel not separately HIL tested

## Known issues

- Params region must be independently erasable (typically one flash sector).
- CRC32 is not cryptographic — does not protect against malicious firmware. Add signature verification at the application layer.
- No built-in flash wear leveling — params are erased+rewritten on every state transition.
- Boot count rollback is based on boot_event calls, not wall time. If bootloader doesn't call boot_event, rollback won't trigger.

## How to test

```bash
cd wheels/protocols/ota_ab/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ota_ab
```

78 assertions across 24 test cases: fresh init, saved params restore, mark pending, size validation (0 and oversized), slot switching on boot, confirm clears trial, rollback after max boots, idle boot noop, slot addresses, CRC32 test vector, null safety, flash write failure, full OTA cycle (A→B→confirm, B→A→rollback), double-confirm no-op, flash read fail on init, flash erase fail on pending, corrupted CRC recovery, persistence across re-init, mark pending overwrite, invalid active_slot recovery, exact boot_count boundary (rollback at max+1), target_slot bidirectional, CRC32 IEEE 802.3 known vector.

## Test status

- Unit tests (sim): PASS (24 C tests, 78 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-28
- **Key decisions**: Flash HAL injection (erase/write/read callbacks); layout struct parametrizes all addresses (no hardcoded sectors); CRC32 bitwise (no table, saves 1KB RAM); boot_event called by bootloader, confirm called by app; max_boot_count configurable.

## Responsible human
@user
