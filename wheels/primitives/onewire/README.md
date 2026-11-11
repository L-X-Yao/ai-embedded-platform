# onewire -- 1-Wire bus protocol (Dallas/Maxim)

**Status: candidate**
**Layer: L0 (primitives)**

GPIO bit-bang implementation of the Dallas/Maxim 1-Wire bus protocol. Handles reset/presence detection, bit and byte I/O (LSB first, standard speed timing), ROM commands (MATCH_ROM, SKIP_ROM, READ_ROM), and CRC-8 validation. Device-specific logic (DS18B20, DS2401, iButton, etc.) belongs in L1 drivers that use this wheel.

## Origin

Proactive build for DS18B20 temperature sensor and other 1-Wire device drivers.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-07):

- Most 1-Wire libraries are tightly coupled to specific MCU peripherals (Arduino OneWire, STM32 HAL OW).
- No suitable zero-dependency C99 bit-bang implementation with DI-based GPIO found.

Reference: Dallas/Maxim AN126 (1-Wire Communication Through Software), DS18B20 datasheet timing specs.

## Dependencies

None.

## API

```c
/* GPIO HAL -- caller provides these via function pointers */
typedef struct {
    void (*pin_low)(void *ctx);               /* drive DQ low        */
    void (*pin_release)(void *ctx);           /* release DQ (float)  */
    bool (*pin_read)(void *ctx);              /* read DQ state       */
    void (*delay_us)(void *ctx, uint16_t us); /* blocking us delay   */
    void *ctx;
} onewire_gpio_t;

/* Bus instance */
typedef struct {
    const onewire_gpio_t *gpio;
} onewire_t;

/* Core API */
onewire_err_t onewire_init(onewire_t *bus, const onewire_gpio_t *gpio);
onewire_err_t onewire_reset(onewire_t *bus, bool *presence);
onewire_err_t onewire_write_byte(onewire_t *bus, uint8_t byte);
onewire_err_t onewire_read_byte(onewire_t *bus, uint8_t *byte);
onewire_err_t onewire_write(onewire_t *bus, const uint8_t *data, uint16_t len);
onewire_err_t onewire_read(onewire_t *bus, uint8_t *buf, uint16_t len);

/* ROM commands */
onewire_err_t onewire_select(onewire_t *bus, const uint8_t rom[8]);   /* MATCH_ROM  */
onewire_err_t onewire_skip(onewire_t *bus);                            /* SKIP_ROM   */
onewire_err_t onewire_read_rom(onewire_t *bus, uint8_t rom[8]);       /* READ_ROM   */

/* Utility */
uint8_t onewire_crc8(const uint8_t *data, uint16_t len);  /* Dallas CRC-8 */
```

## Configuration (compile-time)

No compile-time configuration. All timing constants are internal.

## Supported platforms

- [x] Host (sim) -- 2026-05-07
- [ ] Real hardware target -- not yet verified

## Known issues

- SEARCH_ROM (0xF0) is defined as a constant but not implemented as a function. Device enumeration can be added as a separate function when needed.
- Overdrive speed mode is not supported (standard speed only).

## How to test

```bash
cd wheels/primitives/onewire/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_onewire
./build/test_null_onewire
```

## Test status

- Unit tests (sim): 62 pass, 0 fail
- Null parameter tests: 15 pass, 0 fail
- HIL: N/A

## Migration

### Reset and read_rom report a dead bus (2026-11)

- **What changed:** `onewire_reset` returns `ONEWIRE_ERR_PRESENCE` when no device answers, where it returned `ONEWIRE_OK` with `*presence` false. The new `ONEWIRE_ERR_BUS` comes from `onewire_reset` when the line stays low for 250 us before the pulse, and no pulse is then sent, and from `onewire_read_rom` for an all-zero ROM, whose CRC is valid. `*presence` is still written, and is true only with `ONEWIRE_OK`.
- **What a caller has to do:** Treat `ONEWIRE_ERR_PRESENCE` as an empty bus if that is not a fault for you. A test mock with scripted reads needs one more read, high, ahead of the presence sample.
- **If they do nothing:** `err == ONEWIRE_OK && presence` works as before, and a shorted bus is now refused. Code that returns on any error now reports an empty bus as one.

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: LSB-first bit ordering per 1-Wire spec. CRC-8 uses reflected polynomial 0x8C. read_rom includes automatic CRC validation. All GPIO access through injected function pointers (pin_low/pin_release/pin_read/delay_us).

## Responsible human

@L-X-Yao
