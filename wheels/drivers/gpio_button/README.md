# gpio_button — button debounce and gesture driver

**Status: candidate**

Tick-driven button state machine with debounce, click, double-click, and long-press detection.
Single HAL callback, zero hardware dependency, no dynamic allocation.

## Origin

Extracted from common embedded button patterns. Proactive build for `projects/flybox/` and `projects/drone_light/` platform.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Tightly coupled to GPIO HAL. Self-written with HAL injection per platform conventions.

## Dependencies

- None (interface dependency: `gpio_button_hal_t` read callback provided by caller)

## API

```c
/* HAL: caller provides a read-level function */
gpio_button_hal_t hal = {
    .read = my_gpio_read,   /* returns 0 or 1 */
    .ctx  = &my_pin,
};

/* Configuration (or use defaults) */
gpio_button_cfg_t cfg = gpio_button_default_cfg();
/* Defaults: debounce=20ms, long_press=1000ms, double_click=300ms, active_low */

gpio_button_t btn;
gpio_button_init(&btn, &hal, &cfg);

/* Call periodically (1-10ms recommended) */
gpio_button_evt_t evt = gpio_button_tick(&btn, 1);

switch (evt) {
case GPIO_BUTTON_EVT_PRESS:        /* raw press edge (debounced) */
case GPIO_BUTTON_EVT_RELEASE:      /* raw release edge */
case GPIO_BUTTON_EVT_CLICK:        /* short press + release (after dc window) */
case GPIO_BUTTON_EVT_DOUBLE_CLICK: /* two clicks within dc window */
case GPIO_BUTTON_EVT_LONG_PRESS:   /* held ≥ long_press_ms */
default: break;
}

/* Query current state */
if (gpio_button_is_pressed(&btn)) { /* ... */ }
```

## Supported platforms

- [x] Host (sim) — passed 2026-04-27
- [ ] Real hardware — not yet verified (no project consumer)

## Known issues

- CLICK event is delayed by `double_click_ms` (default 300ms) to distinguish from double-click. Set `double_click_ms=0` to disable double-click and get immediate clicks (not yet supported — would need code change).
- No triple-click or hold-duration reporting. Add if a project needs them.
- Tick resolution affects debounce accuracy. 1ms ticks give ±1ms jitter.

## How to test

```bash
cd wheels/drivers/gpio_button/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_gpio_button
```

44 assertions across 17 test cases: config defaults, idle, debounce, bounce rejection, click, double-click, long press, active-high, long-press vs double-click, null safety, zero-dt, separated rapid clicks, re-init state reset, debounce boundary, barely-miss long press (hold_cnt=999), triple click (double-click + click sequence), large dt behavior.

## Test status

- Unit tests (sim): PASS (17 C tests, 44 assertions)
- HIL: N/A

## API convention deviations

- **`gpio_button_init` returns void** instead of an error code. Reason: init only copies caller-provided config into the instance struct — no I2C transactions or hardware probes that could fail. NULL inputs are silently ignored (no-op).
- **Internal state fields exposed** in `gpio_button_t`. Reason: caller-allocated struct requires known size at compile time. Fields are documented as internal — callers must not read or write them directly.

## API review

- **Date**: 2026-04-27
- **Key decisions**: Single `read(ctx)` HAL callback; `tick(dt_ms)` avoids timer dependency; `active_level` supports both active-low and active-high; CLICK delayed by double-click window to allow gesture disambiguation; LONG_PRESS fires while still held (not on release); configurable timings via `gpio_button_cfg_t`.

## Responsible human

@user

## Event priority

| Gesture | Condition | When emitted |
|---------|-----------|--------------|
| PRESS | Debounced press edge | Immediately |
| RELEASE | Debounced release edge | Immediately |
| LONG_PRESS | Held ≥ `long_press_ms` | During hold |
| CLICK | Short press + release | After `double_click_ms` idle |
| DOUBLE_CLICK | Two presses within `double_click_ms` | On second release |

LONG_PRESS suppresses CLICK on subsequent release. DOUBLE_CLICK consumes both pending clicks.
