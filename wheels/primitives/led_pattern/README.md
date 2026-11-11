# led_pattern

**Status: candidate**
**Layer: L0 (primitives)**

LED blink pattern engine. Drive on/off sequences from a descriptor array.
Caller ticks at fixed interval, engine returns current on/off state.
Supports one-shot and looping patterns, multi-step sequences (e.g. SOS).

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). No lightweight zero-dependency C99 implementation found. Existing LED pattern libraries are Arduino C++; no pure-C passive equivalent exists. Self-written to match platform conventions.

## Dependencies

None.

## API

```c
void led_pattern_init(led_pattern_t *p, const led_pattern_cfg_t *cfg);
bool led_pattern_tick(led_pattern_t *p);
bool led_pattern_is_on(const led_pattern_t *p);
bool led_pattern_finished(const led_pattern_t *p);
void led_pattern_restart(led_pattern_t *p);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/primitives/led_pattern/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **6 C cases** — simple blink loop, one-shot, multi-step,
  restart, empty pattern, NULL safety

## API convention deviations

None.

## API review

- Proposal date: 2026-04-29
- Reviewer: @L-X-Yao (batch-generated, pending review)

## Responsible human

@L-X-Yao
