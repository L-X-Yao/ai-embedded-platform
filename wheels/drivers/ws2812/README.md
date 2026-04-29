# ws2812 — WS2812B RGB LED driver

**Status: candidate**

Platform-portable WS2812B single-wire LED driver.
Encodes 8-bit RGB into GRB PWM bit-stream, sends via caller-provided timer HAL.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`ws2812`, `neopixel`, `led_strip`, `rgb_led` — zero results.

VSF provides GPIO and timer HAL primitives but no WS2812 protocol driver.

## Origin

Extracted from `projects/drone_light/Src/ws2812.c` (N32G003, TIM1 PWM bit-bang).
Generalized: removed MCU-specific registers, added HAL callback injection and multi-LED support.

## API

```c
/* Timing config — values depend on MCU clock frequency */
ws2812_timing_t timing = {
    .period_ticks  = 59,   /* 48MHz / 60 = 800kHz */
    .t0h_ticks     = 16,   /* 0-bit high: ~0.33µs */
    .t1h_ticks     = 31,   /* 1-bit high: ~0.65µs */
    .reset_periods = 80,   /* reset: ≥100µs low */
};

/* HAL — caller implements these for their MCU */
ws2812_hal_t hal = {
    .set_compare    = my_set_ccr,      /* write timer compare register */
    .wait_period    = my_wait_period,  /* block until timer overflow */
    .enter_critical = my_disable_irq,
    .exit_critical  = my_enable_irq,
    .ctx            = &my_timer,
};

/* Send colors (8-bit RGB per LED) */
ws2812_color_t colors[] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
ws2812_send(&hal, &timing, colors, 3);

/* Utility: 5-bit to 8-bit brightness scaling (from NCP5623 range) */
uint8_t r8 = ws2812_scale5to8(15);  /* → 123 */
```

## Timing reference

| Clock | period_ticks | t0h_ticks | t1h_ticks |
|-------|-------------|-----------|-----------|
| 48 MHz | 59 | 16 | 31 |
| 72 MHz | 89 | 24 | 47 |
| 168 MHz | 209 | 56 | 109 |

## Dependencies

- None (interface dependency: `ws2812_hal_t` timer callbacks provided by caller)

## Supported platforms

- [x] Host (sim) — passed 2026-04-27
- [ ] N32G003 (drone_light) — original project code verified, wheel not separately HIL tested

## Known issues

- Critical section disables all interrupts for `num_leds × 30µs`. For long chains (>16 LEDs), consider DMA-based approach instead.
- No DMA support — pure CPU bit-bang via timer CCR updates.
- `scale5to8` LUT uses perceptually-weighted mapping, not linear `v * 255 / 31`.

## How to test

```bash
cd wheels/drivers/ws2812/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ws2812
```

383 assertions across 14 test cases: null safety, GRB encoding, bit patterns, multi-LED, large LED chain (30), reset latch timing, critical section, 5-to-8 scaling, multi-clock timing, all 8 RGB corners ({0,255}^3), mid-gray encoding, data-inside-critical-section isolation.

## Test status

- Unit tests (sim): PASS (14 C tests, 383 assertions)
- HIL: N/A

## API convention deviations

- **Stateless API (no instance struct)**: `ws2812_send` takes HAL + timing + colors per call instead of storing HAL in an instance struct. Reason: the entire operation is a single atomic bit-bang burst — there is no persistent state between calls, so an instance struct would add overhead with no benefit.

## API review

- **Date**: 2026-04-27
- **Key decisions**: HAL is `set_compare` + `wait_period` (maps 1:1 to timer register ops); timing is caller-configured (not hardcoded to any clock); color input is 8-bit (not 5-bit — the 5→8 conversion is a utility, not the main path); multi-LED via color array.

## Responsible human
@user
