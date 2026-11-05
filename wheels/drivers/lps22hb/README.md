# lps22hb — ST LPS22HB barometric pressure sensor driver (I2C)

**Status: candidate**
**Layer: L1 (drivers)**

24-bit pressure + 16-bit temperature MEMS sensor. Supports one-shot and
continuous ODR modes (1/10/25/50/75 Hz). Block data update (BDU) enabled
by default for coherent multi-byte reads.

## Origin

Proactive build for barometric pressure sensing (altimeter, weather station).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- STM32 HAL examples tightly coupled to STM32 HAL, not portable.
- No suitable zero-dependency C99 implementation found that matches wheel contract.

Reference: ST DocID027083 (LPS22HB datasheet).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
lps22hb_err_t lps22hb_init(lps22hb_t *dev, const lps22hb_i2c_t *i2c);
lps22hb_err_t lps22hb_read(lps22hb_t *dev, lps22hb_data_t *out);
lps22hb_err_t lps22hb_set_odr(lps22hb_t *dev, lps22hb_odr_t odr);
lps22hb_err_t lps22hb_one_shot_trigger(lps22hb_t *dev);
lps22hb_err_t lps22hb_read_raw_pressure(lps22hb_t *dev, int32_t *raw);
lps22hb_err_t lps22hb_read_raw_temperature(lps22hb_t *dev, int16_t *raw);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `LPS22HB_DEV_ADDR` | `0x5C` | Default I2C address (SDO/SA0 low) |
| `LPS22HB_DEV_ADDR_ALT` | `0x5D` | Alternate I2C address (SDO/SA0 high) |

## Supported platforms

- [x] Host (sim) — 2026-05-06
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against ST DocID027083 Rev 5. Not run on hardware.

- **Matched:** Addresses, WHO_AM_I, register map, ODR codes, output scaling.
- **Fixed:** With BDU over I2C the manual allows only single-byte reads with IF_ADD_INC clear, and needs PRESS_OUT_H read last. The driver used bursts with 0x2A in the middle, and the temperature read never touched 0x2A, so its output froze. The model now holds samples as the manual says.
- **Suspected:** `lps22hb_one_shot_trigger` returns at once and the reads never check ONE_SHOT or STATUS, so trigger then read can return the last sample. The model finishes a one-shot inside the write, which hides this.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/lps22hb/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lps22hb
./build/test_null_lps22hb
```

## Test status

- Unit tests (sim): 30 pass + 10 null-param tests pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Simple init (no config struct needed; ODR set separately). BDU always enabled. One-shot trigger is explicit separate call.

## Responsible human

@user
