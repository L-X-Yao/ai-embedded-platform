# gpio_led — LED state machine driver

**Status: candidate**

Tick-driven LED state machine with blink, breathe (PWM), and pulse patterns.
Single HAL callback, zero hardware dependency, no dynamic allocation.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`led`, `blink`, `breathe`, `indicator` — zero results.

VSF provides GPIO HAL primitives but no LED pattern state machine.

## Origin

Extracted from common patterns in `projects/flybox/` (LED task) and `projects/drone_light/` (WS2812 control loop).
Generalized into a reusable state machine with HAL callback injection.

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

## Dependencies

- None (interface dependency: `gpio_led_hal_t` set callback provided by caller)

## Supported platforms

- [x] Host (sim) — passed 2026-04-27
- [ ] Real hardware — not yet verified (no project consumer)

## Known issues

- Breathe mode requires PWM-capable output for smooth dimming. On plain GPIO, it produces rapid on/off toggling.
- No gamma correction on breathe ramp — perceptually dim at low levels. Add a gamma LUT in the HAL callback if needed.
- Tick resolution limits minimum blink period. With 10ms ticks, periods below ~20ms are unreliable.

## Tests

```bash
cd wheels/drivers/gpio_led/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_gpio_led
```

52 assertions across 14 test cases: init, on/off, blink (basic/asymmetric/repeat/single), pulse, breathe (ramp/continuous), mode switching, deduplication, null safety, zero-dt, custom brightness.

## Test status

- Unit tests (sim): PASS (14 C tests, 52 assertions)
- HIL: N/A

## API review

- **Date**: 2026-04-27
- **Key decisions**: Single `set(ctx, level)` HAL callback covers both GPIO and PWM; `tick(dt_ms)` avoids timer dependency; `brightness` field allows per-LED dimming; `emit` deduplication prevents redundant HAL calls; `repeat=0` means infinite, `repeat=N` means N cycles then auto-off.
