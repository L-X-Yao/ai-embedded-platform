# button_handler

**Status: candidate**
**Layer: L3 (modules)**

High-level button event processor. Detects click, double-click, long-press,
and repeat patterns via periodic tick polling. Register buttons with a read
callback; call `btn_handler_tick()` at a fixed interval (e.g. every 10ms).

Passive design: no OS, no malloc, no timer HW. All timing expressed in tick
counts -- the caller decides the tick rate.

## Origin

Created 2026-05-01 for high-level button UI patterns in embedded projects.

## Why self-written?

Searched GitHub (2026-05-02). Referenced 0x1abin/MultiButton (2.1k stars) (MIT). Uses global linked-list; our passive tick-driven design cleaner. Self-written for platform convention consistency.

## Dependencies

None (takes a `btn_read_fn` callback, no direct wheel dependency).

Relationship to `gpio_button` (L1): `gpio_button` is a single-button driver with
built-in debounce and HAL coupling. `button_handler` is a multi-button pattern
detector that takes any bool-returning read callback -- it can wrap `gpio_button`,
raw GPIO reads, or anything else.

## API

```c
btn_handler_err_t btn_handler_init(btn_handler_t *bh, btn_callback_fn cb, void *ctx,
                                   uint16_t debounce_ticks, uint16_t long_press_ticks,
                                   uint16_t double_click_ticks, uint16_t repeat_ticks);
btn_handler_err_t btn_handler_register(btn_handler_t *bh, uint8_t id,
                                       btn_read_fn read, void *ctx);
void btn_handler_tick(btn_handler_t *bh);
uint8_t btn_handler_count(const btn_handler_t *bh);
```

### Events

| Event | Trigger |
|-------|---------|
| `BTN_EVENT_PRESS` | Debounced press edge |
| `BTN_EVENT_RELEASE` | Debounced release edge |
| `BTN_EVENT_CLICK` | Single press+release, double-click window expired |
| `BTN_EVENT_DOUBLE_CLICK` | Two presses within double-click window |
| `BTN_EVENT_LONG_PRESS` | Held beyond long-press threshold (once) |
| `BTN_EVENT_REPEAT` | Periodic while held after long-press |

### Configuration

All timing in tick counts. Example at 10ms tick rate:

| Parameter | Ticks | Real time |
|-----------|-------|-----------|
| debounce_ticks | 3 | 30ms |
| long_press_ticks | 100 | 1s |
| double_click_ticks | 30 | 300ms |
| repeat_ticks | 20 | 200ms |

Set `repeat_ticks = 0` to disable repeat events.

## Supported platforms

- [x] Linux host (sim) -- passed 2026-05-01

## Known issues

None.

## How to test

```bash
cd wheels/modules/button_handler/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **24 C cases** -- init, null safety, register, max buttons, press/release,
  click, double-click, long-press, repeat, debounce rejection, long-press suppresses
  click, multiple buttons, correct id/event, idle, no buttons, triple click, repeat
  disabled, timing configuration, callback context passthrough

## API convention deviations

None.

## API review

- Proposal date: 2026-05-01
- Reviewer: @user (pending review)

## Responsible human

@user
