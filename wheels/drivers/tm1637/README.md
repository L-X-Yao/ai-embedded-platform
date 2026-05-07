# tm1637 -- TM1637 4-digit 7-segment LED display driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for TM1637 4-digit 7-segment LED display controller.
Custom 2-wire serial protocol (CLK + DIO, NOT standard I2C) implemented via
GPIO bit-bang. Supports brightness control, number formatting, hex digits,
raw segment output, and colon control. Pure C99, no heap, fully reentrant.

## Origin

Proactive build. TM1637 is one of the most common 7-segment display modules
in embedded applications (clocks, counters, temperature displays, industrial
panels). Widely available as 4-digit modules on Taobao/AliExpress.

## Why self-written?

Searched GitHub (2026-05-07). Existing TM1637 libraries are Arduino/C++
(Avishay/TM1637), platform-specific, or use heap allocation. No suitable
zero-dependency C99 implementation with injected GPIO HAL found.

Reference: TM1637 datasheet (Titan Micro Electronics).

## Dependencies

GPIO HAL (injected via `tm1637_gpio_t` callbacks). No other dependencies.

## API

```c
tm1637_err_t tm1637_init(tm1637_t *dev, const tm1637_gpio_t *gpio);
tm1637_err_t tm1637_set_brightness(tm1637_t *dev, uint8_t level, bool on);
tm1637_err_t tm1637_display_segments(tm1637_t *dev, const uint8_t *segments,
                                     uint8_t count);
tm1637_err_t tm1637_display_number(tm1637_t *dev, int16_t number,
                                   bool leading_zeros);
tm1637_err_t tm1637_display_digit(tm1637_t *dev, uint8_t position,
                                  uint8_t digit);
tm1637_err_t tm1637_set_colon(tm1637_t *dev, bool on);
tm1637_err_t tm1637_clear(tm1637_t *dev);
uint8_t tm1637_encode_digit(uint8_t digit);  /* 0-15 hex */
```

GPIO HAL struct:
```c
typedef struct {
    void (*clk_low)(void *ctx);
    void (*clk_high)(void *ctx);
    void (*dio_low)(void *ctx);
    void (*dio_high)(void *ctx);
    bool (*dio_read)(void *ctx);
    void (*delay_us)(void *ctx, uint16_t us);
    void *ctx;
} tm1637_gpio_t;
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `TM1637_DIGITS` | 4 | Number of display digits |

## Supported platforms

- [x] Host (sim) -- passed 2026-05-07
- [ ] ARM Cortex-M -- not yet verified (uses injected GPIO HAL)

## Known issues

- Only 4-digit displays supported (6-digit TM1637 modules exist but are rare).
- No key scan support (TM1637 has key matrix input, not implemented).
- `display_number` clamps to -999..9999 range.

## How to test

```bash
cd wheels/drivers/tm1637/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tm1637
./build/test_null_tm1637
```

51 unit tests + 9 null-safety tests: init (OK, null params, null callbacks,
ACK error), brightness (set, off, out-of-range, null, ACK error), encode digit
(0-9, hex, out-of-range), display segments (OK, partial, null, bounds, ACK),
display number (0, leading zeros, 1234, 42, negative, clamp, single digit),
display digit (OK, colon preservation, bounds, ACK), colon (on, off, null, ACK),
clear (OK, null, ACK), protocol verification (start/stop conditions).

## Test status

- Unit tests (sim): PASS (51 C tests + 9 null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: Separate CLK/DIO callbacks (not a single write_pin) because TM1637 requires independent control of both lines for start/stop conditions; shadow buffer for segment state enables colon toggle without re-sending all digits; auto-increment mode for full flush, fixed-address mode for single-digit updates; ACK error propagation (TM1637_ERR_ACK) for bus fault detection.

## Responsible human

@user
