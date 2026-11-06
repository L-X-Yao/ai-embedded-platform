# max30102

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Maxim MAX30102 pulse oximeter and heart rate
sensor. I2C interface injected via callbacks. Supports heart-rate-only and
SpO2 modes with configurable LED currents and sample rates. FIFO readout
returns 18-bit ADC samples.

## Origin

Proactive build. MAX30102 is widely used in wearable health monitoring
(pulse oximetry, heart rate) across embedded and IoT applications.

## Why self-written?

MAX30102 is a straightforward register-based I2C sensor. No established
open-source C driver matches our zero-dependency wheel contract. Self-written
for platform convention consistency; register map cross-validated against
datasheet (19-7740, Rev 0).

## Dependencies

I2C HAL (injected via `max30102_i2c_t` callbacks). No other dependencies.

## API

```c
max30102_err_t max30102_init(max30102_t *dev, const max30102_i2c_t *i2c);
max30102_err_t max30102_set_mode(max30102_t *dev, max30102_mode_t mode);
max30102_err_t max30102_set_led_current(max30102_t *dev, uint8_t red_ma_x10, uint8_t ir_ma_x10);
max30102_err_t max30102_set_sample_rate(max30102_t *dev, uint16_t rate);
max30102_err_t max30102_read_fifo(max30102_t *dev, max30102_sample_t *samples,
                                  uint8_t max_count, uint8_t *out_count);
max30102_err_t max30102_read_temp(max30102_t *dev, float *temp_c);
max30102_err_t max30102_shutdown(max30102_t *dev, bool enable);
max30102_err_t max30102_reset(max30102_t *dev);
uint8_t max30102_read_part_id(max30102_t *dev);
```

Modes: `MAX30102_MODE_HR` (RED LED only), `MAX30102_MODE_SPO2` (RED + IR LEDs).

Output: `max30102_sample_t { uint32_t red; uint32_t ir; }` (18-bit ADC values).

## Configuration (compile-time)

None. Mode, LED currents, and sample rate are runtime-configurable.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-07
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against Maxim MAX30102 data sheet, 19-7740; Rev 0; 9/15. Not run on hardware.

- **Matched:** I2C address, part ID, register addresses, MODE and SPO2_CONFIG fields, sample rate codes, FIFO sample format and read order, the temperature formula.
- **Fixed:** `max30102_read_fifo` reported a full FIFO as empty. `max30102_set_sample_rate` accepted rates the pulse width cannot reach, and the chip then ran slower with no error. `max30102_set_mode` left old samples in the FIFO, which misaligned every later sample, and `max30102_init` cleared the pointers before the mode write that makes FIFO_WR_PTR writable. The default LED register was 0x20, while Table 8 gives 6.4 mA at 0x1F. `max30102_init` accepted a NULL `delay_ms`, which made `max30102_read_temp` return the previous result. The driver cited 19-6368 Rev 1, a number this manual does not carry.
- **Still open:** A FIFO that has just reached 32 samples with none lost reads as empty until the next sample. `max30102_read_fifo` does not report lost samples, and the manual does not say how OVF_COUNTER and the pointers behave once a FIFO with rollover on overruns, which init sets, so check that on hardware. `max30102_reset` loses all configuration, leaves MODE at 000, which Table 4 marks Do not use, and keeps the old `dev->mode`: call `max30102_init` after it. Its 100 ms wait is fixed, because the manual gives no reset time and the RESET bit is not polled. Status registers are never read, so PWR_RDY keeps INT low after power-up and a brownout, which also loses the configuration, goes unnoticed. There is no API for pulse width, ADC range or averaging, so the rate stays capped at 400 sps in SpO2 mode and 1000 in HR mode, and none for proximity mode. No test uses `hw_model_max30102.h`: its reset keeps the configuration, it cannot represent a full or overflowed FIFO, it moves the read pointer on the last byte of a sample instead of the first, and it has no time base.

## Known issues

- Multi-LED mode (0x07) is not implemented; only HR and SpO2 modes are supported.
- FIFO sample averaging is set to 1x (no averaging); user can modify FIFO_CONFIG
  register directly if needed.
- LED current API uses uint8_t (max 25.5 mA); full 50 mA range requires uint16_t
  parameter (not exposed to keep API simple for typical use cases).
- SpO2/heart-rate algorithms (R-value calculation, peak detection) are out of scope
  for this driver; application layer should implement signal processing.

## How to test

```bash
cd wheels/drivers/max30102/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_max30102
```

49 test cases covering: init OK, wrong part ID rejected, I2C failures on init,
null safety (15 paths), default register configuration, mode switching (HR/SpO2),
FIFO clear after every mode write, invalid mode rejection, LED current
configuration, all 8 sample rates against Tables 11 and 12 for both modes and all
pulse widths, sample rate preserves other SPO2_CONFIG bits, FIFO read with SpO2
sample parsing, 18-bit ADC masking, empty FIFO, full FIFO, HR-mode FIFO (3 bytes),
max_count limiting, FIFO pointer wrap-around, temperature read (positive and
negative), shutdown enter/exit with mode preservation, reset, part ID readback,
missing callbacks.

## Test status

- Unit tests (sim): PASS (49 C tests + 13 generated null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: Context struct (`max30102_t`) holds I2C HAL + current mode;
  init verifies part ID and resets the device; FIFO read auto-detects bytes per
  sample from mode (3 for HR, 6 for SpO2); LED current in 0.1 mA units avoids
  floating-point API; sample rate validated against the 8 allowed values.

## Responsible human
@user
