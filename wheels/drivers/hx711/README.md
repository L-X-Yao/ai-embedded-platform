# hx711 — GPIO-based 24-bit ADC driver for load cells

**Status: candidate**
**Layer: L1 (drivers)**

Bit-bang driver for the HX711 load cell amplifier/ADC. Clocks 24-bit data out via
GPIO (DOUT + PD_SCK), supports gain selection (32/64/128), tare, and scale conversion.
Pure C99, no OS, no malloc. HAL-injected GPIO + delay callbacks.

## Origin

Proactive build for weight/force measurement applications using load cells.
## Dependencies

None (GPIO HAL injected by caller).

## API

```c
/* Error codes */
HX711_OK, HX711_ERR_PARAMS, HX711_ERR_TIMEOUT, HX711_ERR_NOT_READY

/* Gain selection (also sets channel) */
HX711_GAIN_128  /* Ch A, 128x, 25 pulses */
HX711_GAIN_32   /* Ch B,  32x, 26 pulses */
HX711_GAIN_64   /* Ch A,  64x, 27 pulses */

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

@user
