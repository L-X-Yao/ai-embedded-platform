# pct2075 — NXP PCT2075 digital temperature sensor

**Status: candidate**
**Layer: L1 (drivers)**

11-bit I2C temperature sensor, LM75 successor with higher resolution (0.125 C vs 0.5 C).
Range -55 C to +125 C. Configurable address via A0-A2 pins: 0x28-0x2F, 0x35-0x37, 0x48-0x4F or 0x70-0x77.

## Origin

Proactive build for temperature sensing use cases (ADR-011 ecosystem-driven).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- Existing LM75 drivers cover 9-bit only; PCT2075 needs 11-bit (shift-right-5) conversion.
- No suitable zero-dependency C99 implementation found that matches our wheel contract.

Reference: NXP PCT2075 datasheet.

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
pct2075_err_t pct2075_init(pct2075_t *dev, const pct2075_i2c_t *i2c,
                            const pct2075_cfg_t *cfg);
pct2075_err_t pct2075_read(pct2075_t *dev, float *temp_c);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `CONTRACT_ENABLED` | off | Enable Design-by-Contract checks |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against NXP PCT2075 Rev. 9 (24 October 2014). Not run on hardware.

- **Matched:** Pointer values, register widths, the 11-bit temperature format and the Table 12 values, byte order, the -55 C to +125 C range, no chip ID.
- **Fixed:** `pct2075_init` refused every address outside 0x48-0x4F, so a board with an address pin left floating could not start. All 27 addresses in Table 5 now pass.
- **Still open:** The model leaves Tos and Thyst at 0 instead of 5000h and 4B00h, and shares one byte between the Temp LSB and Conf, so a Conf write changes its temperature. Its header also gives T_idle as 0x01, where Table 7 says 00h. Two tests label conf_val 0x04 as fault queue 2 and 0x0C as queue 6, but the queue is in B[4:3], so 0x04 only sets OS_POL. None of this reaches hardware, but the tests cannot catch a Conf mistake. There is no API for Tos, Thyst or Tidle, so the thermal limits stay at their defaults, and no named Conf bits, so shutdown and the OS output need a raw `conf_val` at init.
- **Suspected:** `pct2075_init` does not wait, so a read within about 28 ms of power-up returns 0 C with `PCT2075_OK`.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/pct2075/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pct2075
./build/test_hw_model_pct2075
./build/test_null_pct2075
```

## Test status

- Unit tests (sim): 38 tests, all passing (23 mock + 10 hw_model + 5 null)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: 2-function API (init + read), cfg struct for address and optional config write, error enum OK=0/ERR_I2C=-1/ERR_PARAM=-2

## Responsible human

@user
