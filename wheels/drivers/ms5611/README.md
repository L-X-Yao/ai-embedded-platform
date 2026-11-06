# ms5611

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for TE Connectivity MS5611 precision barometric
pressure sensor. I2C interface injected via callbacks. Implements the
factory-calibrated compensation algorithm from the MS5611-01BA03 datasheet
including 2nd-order correction for low temperatures.

## Origin

Proactive build (ADR-011). MS5611 is a precision barometer widely used in
drones, altimeters, and weather stations for its 10 cm altitude resolution.

## Why self-written?

Searched GitHub (2026-05-11). No single-file zero-dependency C99 driver
matching our wheel contract. Self-written; compensation formulas from
MS5611-01BA03 datasheet.

## Dependencies

I2C HAL (injected via `ms5611_i2c_t` callbacks). `read_regs`, `delay_ms` and
`write_raw` are required. `write_reg` is no longer used and may be NULL. No
other dependencies.

## API

```c
ms5611_err_t ms5611_init(ms5611_t *dev, const ms5611_i2c_t *i2c,
                         const ms5611_cfg_t *cfg);
ms5611_err_t ms5611_read(ms5611_t *dev, ms5611_data_t *out);
ms5611_err_t ms5611_reset(ms5611_t *dev);
```

Output: `ms5611_data_t { float temperature_c; float pressure_pa; }`

## Configuration (compile-time)

None. OSR and I2C address are runtime-configurable via `ms5611_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against MS5611-01BA03, 06/2017. It prints no document number or revision beyond this, and AN520, which holds the CRC code, is not in the manual set. Not run on hardware.

- **Matched:** Addresses, commands, reset and OSR waits, PROM and ADC byte order, first and second order compensation, units.
- **Fixed:** The PROM CRC was read from the top nibble of word 7, so a real part would fail `ms5611_init` with `MS5611_ERR_CRC`. It now comes from DB3-DB0 (Figure 16), and the low byte of word 7 is cleared for the calculation. The mask and the CRC-4 loop were checked against ArduPilot's `AP_Baro_MS5611.cpp` and `crc_crc4`, and the test vector is the PROM in its `SIM_MS5611.h`. `ms5611_init` refuses a HAL without `delay_ms`. `write_reg(cmd, 0)` put a 0x00 data byte after every Reset and Convert command, where Figures 10 and 13 end the message at the command. 0x00 is ADC Read, and an ADC read during conversion gives 0 and spoils the result. Commands now go out as one byte through the new `write_raw` callback, and `ms5611_init` returns `MS5611_ERR_PARAM` without it. An all-zero PROM passes the CRC, so a bus that reads zeros got through init. `ms5611_init` now refuses it with `MS5611_ERR_CRC`, as ArduPilot does. The model discarded the data byte and no test built it. `test_hw_model_ms5611.c` now builds it and reads the p8 example through the driver. `read_adc` took a 0 as a measurement, so a missed conversion came back as data with `MS5611_OK`. `ms5611_read` now returns `MS5611_ERR_ADC`. The model has a clock: a conversion lasts the p3 maximum for its OSR, an ADC read or Convert during it spoils it, each result goes out once, and the PROM loads 2.8 ms after Reset. The mock gives 0 without a Convert and on a repeated read. None of these fixes has run on hardware.
- **Still open:** No SPI, no bus recovery, no access to the serial code, one OSR for both conversions, and `ms5611_read` blocks through both.
- **Suspected:** `read_regs` may join command and data with a repeated START, where Figures 11, 12, 14 and 15 show two messages.

## Known issues

- Command-based protocol uses read_regs as the transport for PROM and ADC
  reads; actual MS5611 protocol is different from standard register I2C.
- Only I2C is implemented (MS5611 also supports SPI).

## How to test

```bash
cd wheels/drivers/ms5611/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ms5611
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
