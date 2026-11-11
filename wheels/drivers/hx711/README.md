# hx711 — GPIO-based 24-bit ADC driver for load cells

**Status: candidate**
**Layer: L1 (drivers)**

Bit-bang driver for the HX711 load cell amplifier/ADC. Clocks 24-bit data out via
GPIO (DOUT + PD_SCK), supports gain selection (32/64/128), tare, and scale conversion.
Pure C99, no OS, no malloc. HAL-injected GPIO + delay callbacks.

## Origin

Proactive build for weight/force measurement applications using load cells.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Bit-bang GPIO timing, platform-specific. Self-written with HAL injection per platform conventions.

## Dependencies

None (GPIO HAL injected by caller).

## API

```c
/* Error codes: HX711_OK=0, HX711_ERR_PARAM=-1, HX711_ERR_TIMEOUT=-2,
 *              HX711_ERR_NOT_READY=-3 */

/* Gain selection (also sets channel):
 *   HX711_GAIN_128  — Ch A, 128x, 25 pulses
 *   HX711_GAIN_32   — Ch B,  32x, 26 pulses
 *   HX711_GAIN_64   — Ch A,  64x, 27 pulses */

/* HAL callbacks */
typedef struct {
    bool (*read_dout)(void *ctx);
    void (*set_sck)(void *ctx, bool level);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} hx711_hal_t;

/* Core API */
hx711_err_t hx711_init(hx711_t *dev, const hx711_hal_t *hal, hx711_gain_t gain);
bool        hx711_is_ready(const hx711_t *dev);
hx711_err_t hx711_read_raw(hx711_t *dev, int32_t *value);
hx711_err_t hx711_read(hx711_t *dev, float *value);     /* (raw - offset) / scale */
hx711_err_t hx711_tare(hx711_t *dev, uint8_t samples);
void        hx711_set_scale(hx711_t *dev, float scale);
void        hx711_set_offset(hx711_t *dev, int32_t offset);
void        hx711_power_down(hx711_t *dev);
void        hx711_power_up(hx711_t *dev);
```

## Configuration (compile-time)

No compile-time defines. All configuration via runtime API (gain, scale, offset).

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against HX711 24-Bit ADC for Weigh Scales, Avia Semiconductor (no document number or revision printed). Not run on hardware.

- **Matched:** DOUT polarity, 24 data bits MSB first, the 25 to 27 pulses of Table 3, SCK timing, sign extension, the power-down hold.
- **Fixed:** `hx711_tare` failed on its second sample, because DOUT stays high until the next conversion ends. It now waits for each sample and returns `HX711_ERR_TIMEOUT` after 1.2 s. The first read after init or power up was Channel A at gain 128 whatever `gain` said, so both now do one read to set it. The mock hid both, so it now keeps conversion time and the selected input.
- **Still open:** Saturation at 800000h or 7FFFFFh comes back as a normal value with no flag. Gain and channel change only through `hx711_init`, which also clears offset and scale. `hx711_power_up` returns nothing, so it cannot report a timeout, but the next read then returns `HX711_ERR_NOT_READY`. There is no hw_model, only the mock.
- **Suspected:** Nothing waits out the settling time after power up or a gain change, 400 ms at RATE=0 or 50 ms at RATE=DVDD, so the first results may be unsettled. An interrupt that holds SCK high past 60 us powers the chip down in mid-read, and the frame then comes back wrong with `HX711_OK`.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/hx711/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hx711
```

## Test status

- Unit tests (sim): 18 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Custom HAL (read_dout/set_sck/delay_us) instead of generic GPIO
  template, because HX711 uses a proprietary serial protocol not covered by standard
  I2C/SPI/GPIO patterns.

## Responsible human

@L-X-Yao
