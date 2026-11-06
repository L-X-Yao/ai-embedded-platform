# ccs811

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for AMS CCS811 indoor air quality sensor.
I2C interface injected via callbacks. Reads eCO2 (400-8192 ppm) and
TVOC (0-1187 ppb). Implements boot sequence and drive mode configuration.

## Origin

Proactive build (ADR-011). CCS811 is a popular VOC/eCO2 sensor for
indoor air quality monitoring in HVAC and smart home applications.

## Why self-written?

Searched GitHub (2026-05-11). No single-file zero-dependency C99 driver
matching our wheel contract. Self-written; protocol from CCS811 datasheet.

## Dependencies

I2C HAL (injected via `ccs811_i2c_t` callbacks). `write_reg`, `read_regs` and
`write_raw` are required, and `ccs811_init` returns `CCS811_ERR_PARAM` without
them. `write_raw` puts exact bytes on the bus, which APP_START and ENV_DATA
need. `delay_ms` is optional. No other dependencies.

## API

```c
ccs811_err_t ccs811_init(ccs811_t *dev, const ccs811_i2c_t *i2c,
                         const ccs811_cfg_t *cfg);
ccs811_err_t ccs811_read(const ccs811_t *dev, ccs811_data_t *out);
ccs811_err_t ccs811_set_mode(ccs811_t *dev, ccs811_drive_mode_t mode);
ccs811_err_t ccs811_set_env_data(const ccs811_t *dev, uint16_t humidity,
                                 uint16_t temperature);
```

Output: `ccs811_data_t { uint16_t eco2_ppm; uint16_t tvoc_ppb; }`

## Configuration (compile-time)

None. Drive mode and I2C address are runtime-configurable via `ccs811_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against ams CCS811 datasheet v1-00. Not run on hardware.

- **Matched:** I2C addresses, HW_ID, register addresses, STATUS bits, drive mode codes, result byte order, the APP_VALID and FW_MODE checks.
- **Fixed:** `ccs811_read` returned old values in the 250 ms raw mode, where the chip leaves ALG_RESULT_DATA alone. It now returns `CCS811_ERR_MODE`. APP_START went out with a data byte the manual does not allow. It now goes out as the single byte 0xF4 through the new `write_raw` callback, and `ccs811_init` returns `CCS811_ERR_PARAM` without that callback. `ccs811_set_env_data` sent two single bytes, one to a register that does not exist, and a caller following the header put 0x00 on the bus. It now sends register 0x05 and 4 data bytes in one write, with each argument in the high byte of its field, so 100 is 0x64, 0x00 as on p19. A value above 255 returns `CCS811_ERR_PARAM`. The mock began with FW_MODE set and the model started the application on any write to 0xF4, which hid this. Both now need the lone 0xF4, and `test_hw_model_ccs811.c` builds the model. `ccs811_init` also sent 0xF4 to a chip already running, where the application map has no such register, and now skips it. The model and the mock now treat registers as mailboxes, and the model knows both register maps, takes a sample at each measurement interval, clears DATA_READY on a read and leaves ALG_RESULT_DATA alone in mode 4. None of these fixes has run on hardware.
- **Suspected:** A change to a slower mode should pass through idle for 10 minutes. The STATUS error bit is never read. There is no wait after power-on.

## Known issues

- `ccs811_set_env_data` takes the 0.5 steps its header documents, so the
  part of ENV_DATA below 0.5 is always 0. Finer steps would change what the
  arguments mean.
- Baseline save/restore not implemented.

## How to test

```bash
cd wheels/drivers/ccs811/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ccs811
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## API Review

Pending.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
