# ds18b20

**Status: candidate**
**Layer: L1 (drivers)**

Dallas/Maxim DS18B20 1-Wire digital temperature sensor driver. 12-bit
resolution (0.0625°C), configurable 9-12 bits. Supports single-device
(skip ROM) and multi-device (match ROM) buses.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Referenced libdriver/ds18b20, DallasOneWire (MIT) for register sequences and compensation algorithms. 1-Wire timing platform-critical; algorithm reference. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

1-Wire HAL (injected via callbacks).

## API

```c
#include "ds18b20.h"

ds18b20_err_t ds18b20_init(ds18b20_t *dev, const ds18b20_ow_t *ow);
ds18b20_err_t ds18b20_read_rom(ds18b20_t *dev);
ds18b20_err_t ds18b20_start_conversion(ds18b20_t *dev);
ds18b20_err_t ds18b20_read_temperature(ds18b20_t *dev, float *temp_c);
ds18b20_err_t ds18b20_set_resolution(ds18b20_t *dev, ds18b20_resolution_t res);
uint8_t       ds18b20_crc8(const uint8_t *data, size_t len);
```

HAL: 1-Wire (reset + write_byte + read_byte).

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Datasheet check

Checked on 2026-11-06 against DS18B20, REV: 042208 (Maxim, 2008). Not run on hardware.

- **Matched:** The commands and their order, family code 28h in the tests, the temperature format and byte order, the configuration byte values, the scratchpad layout, what each CRC covers.
- **Fixed:** The CRC table did not follow the polynomial, so a real chip would fail almost every read with `DS18B20_ERR_CRC`. The model and the mock used the same table and hid it, so both now compute the CRC from the polynomial. Below 12 bits the undefined low bits reached the temperature, up to 0.4375 C at 9 bits. The model answered any ROM code, ignored Write Scratchpad and gave a temperature with no Convert T, and the mock had the wrong reserved bytes and stored nothing, so tests now see the ROM match, the byte order, the resolution and the conversion time.
- **Still open:** Nothing waits for a conversion, so a read before its end returns the old value, or 85 C after power-up, with `DS18B20_OK`. `ds18b20_read_rom` does not check the family code, and an all-zero frame has CRC 0, so a bus held low reads as 0.0 C with `DS18B20_OK`. There is no Search ROM, so a multi-device bus needs `rom` and `use_rom` filled by hand. There is no Copy Scratchpad, so `ds18b20_set_resolution` lasts until power-down. Parasite power needs a strong pullup, which takes a new HAL callback.
- **Suspected:** `ds18b20_set_resolution` writes 0 to TH and TL, which clears alarm limits or user data until the next power-up. Four functions are tagged `@isr_safe: yes`, although a temperature read takes about 6 ms of bus time.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ds18b20/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **9 C cases** — init null, 25°C read, negative temp, 85°C power-on
  default, no device, CRC failure, read ROM, CRC8 known vector, start conversion

## API convention deviations

None.

## API review

- Proposal date: 2026-04-29
- Reviewer: @L-X-Yao (batch-generated, pending review)

## Responsible human

@L-X-Yao
