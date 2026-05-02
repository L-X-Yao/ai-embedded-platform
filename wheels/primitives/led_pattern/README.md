# led_pattern

**Status: candidate**
**Layer: L0 (primitives)**

LED blink pattern engine. Drive on/off sequences from a descriptor array.
Caller ticks at fixed interval, engine returns current on/off state.
Supports one-shot and looping patterns, multi-step sequences (e.g. SOS).
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

## How to test

```bash
cd wheels/primitives/led_pattern/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Origin

Proactive build for common embedded components (2026-04-29 batch).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Known issues

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
