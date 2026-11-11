# debounce

**Status: candidate**
**Layer: L0 (primitives)**

Software button debouncing. Feed raw pin state at fixed intervals; output
transitions to new stable state after N consecutive identical readings.
Provides rising/falling edge detection.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Referenced 0x1abin/MultiButton (2.1k stars, MIT) for design patterns. MultiButton is a full button driver; our wheel is a pure debounce primitive without GPIO or callback coupling. Self-written for platform convention consistency; algorithm cross-validated against reference.

## Dependencies

None.

## API

```c
void debounce_init(debounce_t *d, uint8_t threshold, bool initial_state);
bool debounce_update(debounce_t *d, bool raw);
bool debounce_get(const debounce_t *d);
bool debounce_changed(const debounce_t *d);
bool debounce_rising(const debounce_t *d);
bool debounce_falling(const debounce_t *d);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/primitives/debounce/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — init, threshold crossing, bounce rejection, falling edge,
  stable no-change, threshold 1, NULL safety, rapid toggle

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@L-X-Yao
