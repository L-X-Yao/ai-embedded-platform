# gpio_led — LED state machine driver

**Status: candidate**

Tick-driven LED state machine with blink, breathe (PWM), and pulse patterns.
Single HAL callback, zero hardware dependency, no dynamic allocation.

## Origin

Extracted from common patterns in `projects/flybox/` (LED task) and `projects/drone_light/` (WS2812 control loop).
Generalized into a reusable state machine with HAL callback injection.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Trivial GPIO control. Self-written with HAL injection per platform conventions.

## Dependencies

- None (interface dependency: `gpio_led_hal_t` set callback provided by caller)

## API

```c
/* HAL: caller provides a set-level function */
gpio_led_hal_t hal = {
    .set = my_gpio_set,   /* level: 0=off, 1-255=brightness */
    .ctx = &my_pin,
};

gpio_led_t led;
gpio_led_init(&led, &hal);

/* Static states */
gpio_led_on(&led);
gpio_led_off(&led);

/* Blink: 500ms on, 500ms off, infinite */
gpio_led_blink(&led, 500, 500, 0);

/* Blink 3 times then stop */
gpio_led_blink(&led, 200, 200, 3);

/* Breathe: 2s triangle wave (needs PWM output) */
gpio_led_breathe(&led, 2000);

/* Pulse: on for 100ms then auto-off */
gpio_led_pulse(&led, 100);

/* Call periodically from main loop or timer ISR */
gpio_led_tick(&led, 10);  /* 10ms elapsed */

/* Custom brightness (default 255) */
led.brightness = 128;
gpio_led_on(&led);  /* outputs 128 */
```

## Supported platforms

- [x] Host (sim) — passed 2026-04-27
- [ ] Real hardware — not yet verified (no project consumer)

## Known issues

- Breathe mode requires PWM-capable output for smooth dimming. On plain GPIO, it produces rapid on/off toggling.
- No gamma correction on breathe ramp — perceptually dim at low levels. Add a gamma LUT in the HAL callback if needed.
- Tick resolution limits minimum blink period. With 10ms ticks, periods below ~20ms are unreliable.

## How to test

```bash
cd wheels/drivers/gpio_led/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_gpio_led
```

70 assertions across 21 test cases: init, on/off, blink (basic/asymmetric/repeat/single/zero-on/zero-off), pulse (normal/zero-on), breathe (ramp/continuous/period-one/period-zero/large-dt), mode switching, deduplication, null safety, zero-dt, custom brightness, re-init reset.

## Test status

- Unit tests (sim): PASS (21 C tests, 70 assertions)
- HIL: N/A

## API convention deviations

- **All API functions return void** instead of error codes. Reason: operations are pure state-machine transitions on a caller-allocated struct — no I2C transactions or hardware probes that could fail. NULL inputs are silently ignored (no-op).
- **Internal state fields exposed** in `gpio_led_t`. Reason: caller-allocated struct requires known size at compile time. Fields are documented as internal — callers must not read or write them directly (except `brightness`, which is a user-settable field).

## API review

- **Date**: 2026-04-27
- **Key decisions**: Single `set(ctx, level)` HAL callback covers both GPIO and PWM; `tick(dt_ms)` avoids timer dependency; `brightness` field allows per-LED dimming; `emit` deduplication prevents redundant HAL calls; `repeat=0` means infinite, `repeat=N` means N cycles then auto-off.

## Responsible human

@user

## Modes

| Mode | Behavior | HAL output |
|------|----------|------------|
| `OFF` | Static off | `0` |
| `ON` | Static on | `brightness` (default 255) |
| `BLINK` | Periodic on/off | `brightness` / `0` |
| `BREATHE` | Triangle ramp (continuous) | `0` → `brightness` → `0` |
| `PULSE` | Single flash, auto-off | `brightness` → `0` |

## Integration

For simple on/off LEDs, `set` writes GPIO (0 or 1). For PWM LEDs, `set` writes duty cycle (0-255). The `emit` deduplication avoids redundant HAL calls.
