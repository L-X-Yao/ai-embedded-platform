# ota_ab — A/B firmware slot manager with rollback

**Status: candidate**
**Layer: L2 (protocols)**

Platform-agnostic A/B firmware slot management for OTA upgrades. Handles slot selection,
parameter persistence with CRC32 protection, and automatic rollback after N failed boots.
Flash operations injected via HAL callbacks — no hardware dependency.

## Origin

Reimplemented for the needs of an internal project.
Decoupled: HTTP download, modem interaction, FreeRTOS task scheduling stay in the project;
pure slot management logic moves to this wheel.

## Why self-written?

Searched GitHub (2026-05-02). No suitable lightweight zero-dependency C99 implementation found. A/B slot metadata management is inherently platform-specific; all existing OTA libraries are OS-coupled. Self-written per platform conventions.

## Dependencies

None. Pure C99 with injected flash HAL callbacks.

## API

```c
static bool my_erase(void *ctx, uint32_t addr, uint32_t size)
    { (void)ctx; (void)addr; (void)size; return true; }
static bool my_write(void *ctx, uint32_t addr, const uint8_t *data, uint32_t len)
    { (void)ctx; (void)addr; (void)data; (void)len; return true; }
static bool my_read(void *ctx, uint32_t addr, uint8_t *buf, uint32_t len)
    { (void)ctx; (void)addr; (void)buf; (void)len; return true; }

static void app_example(void) {
    ota_ab_t m;
    ota_ab_layout_t layout = {
        .slot_a_addr    = 0x08010000,
        .slot_b_addr    = 0x08080000,
        .slot_max_size  = 0x70000,
        .params_addr    = 0x080E0000,
        .params_size    = 4096,
        .max_boot_count = 3,
        .params_addr2   = 0x080E1000,  /* its own erase unit; see Migration */
    };
    const ota_ab_flash_t flash_hal = {
        .erase = my_erase, .write = my_write, .read = my_read, .ctx = NULL
    };
    ota_ab_init(&m, &flash_hal, &layout);

    /* App: after downloading + CRC-verifying new firmware */
    uint32_t fw_size = 0x10000;
    uint32_t fw_crc  = 0xDEADBEEF;
    ota_ab_mark_pending(&m, fw_size, fw_crc);

    /* Bootloader: on every boot */
    ota_ab_boot_event(&m);

    /* App: after confirming new firmware works */
    ota_ab_confirm(&m);
}
```

## Supported platforms

- [x] Host (sim) — passed 2026-04-28
- [ ] GD32F405 (flybox) — original project code verified, wheel not separately HIL tested

## Known issues

- Each params region must be independently erasable (typically one flash sector).
- With `params_addr2 = 0` there is one copy, and power lost between its erase and its write loses the record: the next init falls back to slot 0 with no trial state, and on an update to slot A that boots an unverified image with no rollback. Set `params_addr2` to close this.
- A power cut during a write loses that one write, not the record. If it was `mark_pending`, the update has to be marked again. If it was a boot of a trial, the count of that boot is lost, so rollback comes one boot later.
- CRC32 is not cryptographic — does not protect against malicious firmware. Add signature verification at the application layer.
- No built-in flash wear leveling — params are erased+rewritten on every state transition. Two copies halve the wear on each region.
- Boot count rollback is based on boot_event calls, not wall time. If bootloader doesn't call boot_event, rollback won't trigger.

## Migration

### Second params copy (2026-11)

**What changed.** New layout field `params_addr2`. `ota_ab_init` refuses a value that overlaps the first region with `OTA_AB_ERR_PARAM`. The `reserved` byte of `ota_ab_params_t` is now `seq`, at the same offset, so the record keeps its size and format. `ota_ab_t` is opaque storage and grew by two small fields.

**What a caller has to do.** To survive a power cut, set `params_addr2` to a second region in its own erase unit, `params_size` long. A device already in the field needs no flash migration: the existing record loads as sequence 0 and the first write goes to the new region, leaving it intact. Anything that named `reserved` must say `seq`.

**If you do nothing.** With `params_addr2` left 0 the bytes on flash and the behaviour stay exactly as they were, including the power-cut window under "Known issues".

## How to test

```bash
cd wheels/protocols/ota_ab/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ota_ab
```

45 test cases, among them a power cut in every erase and write of two full update cycles, at every byte: fresh init, saved params restore, mark pending, size validation (0 and oversized), slot switching on boot, confirm clears trial, rollback after max boots, idle boot noop, slot addresses, CRC32 test vector, null safety, flash write failure, full OTA cycle (A→B→confirm, B→A→rollback), double-confirm no-op, flash read fail on init, flash erase fail on pending, corrupted CRC recovery, persistence across re-init, mark pending overwrite, invalid active_slot recovery, exact boot_count boundary (rollback at max+1), target_slot bidirectional, CRC32 IEEE 802.3 known vector.

## Test status

- Unit tests (sim): PASS (45 C tests)
- HIL: N/A

## API convention deviations

None.

## Protocol (A/B boot state machine)

```
IDLE ──[mark_pending]──→ PENDING ──[boot_event]──→ SUCCESS (trial)
                                                       │
                                          [confirm]────┘──→ IDLE (new active)
                                          [N boots]────┘──→ IDLE (rollback to old)
```

## API review

- **Date**: 2026-04-28
- **Key decisions**: Flash HAL injection (erase/write/read callbacks); layout struct parametrizes all addresses (no hardcoded sectors); CRC32 bitwise (no table, saves 1KB RAM); boot_event called by bootloader, confirm called by app; max_boot_count configurable, 1 to 254.

## Responsible human

@user
