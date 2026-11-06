# onewire_search -- Dallas/Maxim 1-Wire ROM search algorithm

**Status: candidate**
**Layer: L2 (protocols)**

Dallas/Maxim 1-Wire ROM search algorithm (AN187). Discovers all devices on a 1-Wire bus
by walking the 64-bit ROM code tree. Includes CRC8 validation of discovered ROM codes.
Zero OS dependency -- bus operations injected via function pointer HAL.

## Origin

Proactive build for 1-Wire sensor networks (DS18B20 temperature arrays, iButton readers).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- Maxim's AN187 reference C code is not distributable (proprietary license).
- OneWireNg (MIT, C++) -- C++ only, pulls in Arduino/platform dependencies.
- owlib (GPL) -- license incompatible.
- No suitable zero-dependency C99 implementation found.

Reference: Dallas/Maxim Application Note 187.

## Dependencies

None. Bus HAL injected via `onewire_search_bus_t` function pointers.

## API

```c
/* Initialize search state */
onewire_search_state_t state;
onewire_search_bus_t bus = { .ctx = &my_hw, .reset = hw_reset,
                              .write_bit = hw_write_bit, .read_bit = hw_read_bit };
onewire_search_init(&state, &bus);

/* Enumerate all devices */
onewire_search_rom_t rom;
while (onewire_search_next(&state, &rom) == ONEWIRE_SEARCH_OK) {
    /* rom.rom[0] = family code, rom.rom[1..6] = serial, rom.rom[7] = CRC */
    process_device(rom.rom);
}

/* Or use search_first for convenience restart */
onewire_search_first(&state, &rom);

/* Check if enumeration is complete */
bool done = onewire_search_done(&state);

/* CRC8 utility */
uint8_t crc = onewire_search_crc8(data, len);
```

## Configuration (compile-time)

No compile-time configuration needed. Constants are fixed by the 1-Wire protocol.

| Define | Default | Purpose |
|--------|---------|---------|
| `ONEWIRE_SEARCH_ROM_LEN` | 8 | ROM code length in bytes (protocol-fixed) |
| `ONEWIRE_SEARCH_CMD_SEARCH` | 0xF0 | SEARCH ROM command (protocol-fixed) |

## Supported platforms

- [x] Host (sim) -- passed 2026-05-12
- [ ] Real hardware (DS18B20 on STM32) -- not yet verified

## Known issues

- Only implements SEARCH ROM (0xF0). ALARM SEARCH (0xEC) not included but uses same algorithm
  with different command byte.
- CRC8 is computed bit-by-bit (no lookup table) -- fast enough for 8-byte ROM codes but not
  optimal for bulk data CRC.
- `ONEWIRE_SEARCH_ERR_CRC` and `ONEWIRE_SEARCH_ERR_BUS` leave the search where it was so
  that the caller can retry, and a fault that lasts keeps returning them: bound any
  retries. No presence before any device is found ends the search as an empty bus,
  and `onewire_search_first` looks again.

## How to test

```bash
cd wheels/protocols/onewire_search/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_onewire_search
```

## Test status

- Unit tests (sim): PASS (29 C tests)
- NULL tests: generated
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: HAL injected via function pointer struct (reset/write_bit/read_bit);
  state struct holds search progress across calls; CRC8 uses reflected polynomial 0x8C
  matching Dallas convention; search_first is convenience wrapper over init+next;
  search_done is const query that does not touch the bus.

## Responsible human

@user
