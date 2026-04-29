# led_pattern

**STATUS: candidate**
**Layer: L0 (primitives)**

LED blink pattern engine. Drive on/off sequences from a descriptor array.
Caller ticks at fixed interval, engine returns current on/off state.
Supports one-shot and looping patterns, multi-step sequences (e.g. SOS).

## Why not VSF native?

Checked VSF source (2026-04-29): no LED pattern engine in VSF.

## API

```c
void led_pattern_init(led_pattern_t *p, const led_pattern_cfg_t *cfg);
bool led_pattern_tick(led_pattern_t *p);
bool led_pattern_is_on(const led_pattern_t *p);
bool led_pattern_finished(const led_pattern_t *p);
void led_pattern_restart(led_pattern_t *p);
```

## Test status

- Unit tests: **6 C cases** — simple blink loop, one-shot, multi-step,
  restart, empty pattern, NULL safety

## Dependencies

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## API convention deviations

无。

## Responsible human

@user
