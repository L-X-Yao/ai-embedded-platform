# soft_timer

**Status: candidate**
**Layer: L0 (primitives)**

Tick-driven software timer manager. Supports one-shot and repeating timers
with per-timer callbacks. Driven by a single `soft_timer_tick()` call from
SysTick or any periodic source. No heap, no framework dependency.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`callback_timer`, `timer`, `soft_timer` — VSF has `vsf_callback_timer_t` in the
kernel, but it requires the full VSF kernel and `vsf_eda_t` task infrastructure.
This wheel is framework-independent and works on bare-metal or any RTOS.

## Origin

Proactive build. Software timers are used in every embedded project: LED blink,
periodic sensor sampling, communication timeouts, debounce delays.

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`.

## API

```c
soft_timer_err_t soft_timer_init(soft_timer_mgr_t *mgr);
int              soft_timer_create(soft_timer_mgr_t *mgr, uint32_t period_ticks,
                                  soft_timer_mode_t mode, soft_timer_cb_t cb,
                                  void *user_ctx);
soft_timer_err_t soft_timer_start(soft_timer_mgr_t *mgr, int id);
soft_timer_err_t soft_timer_stop(soft_timer_mgr_t *mgr, int id);
soft_timer_err_t soft_timer_reset(soft_timer_mgr_t *mgr, int id);
bool             soft_timer_is_active(const soft_timer_mgr_t *mgr, int id);
void             soft_timer_tick(soft_timer_mgr_t *mgr);
uint32_t         soft_timer_get_tick(const soft_timer_mgr_t *mgr);
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `SOFT_TIMER_MAX` | 16 | Maximum number of timers per manager |

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## Tests

```bash
cd wheels/primitives/soft_timer/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_soft_timer
```

46 assertions across 13 test cases: init, null safety (8 paths), create+start,
one-shot fires and auto-stops, repeating fires continuously, stop halts timer,
reset restarts countdown, user_ctx passthrough, multiple independent timers,
invalid ID handling, tick counter, null tick no-crash, period=1 every-tick.

## Test status

- Unit tests (sim): PASS (13 C tests, 46 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Static array (not linked list) for O(1) access and no heap; create returns int ID (not pointer) to avoid dangling pointer issues; timer is not auto-started on create (explicit start required); one-shot auto-deactivates after firing; tick must be called from one context (not ISR-safe by design — caller wraps if needed).

## Responsible human
@user
