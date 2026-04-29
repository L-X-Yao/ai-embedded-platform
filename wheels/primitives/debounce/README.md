# debounce

**STATUS: candidate**
**Layer: L0 (primitives)**

Software button debouncing. Feed raw pin state at fixed intervals; output
transitions to new stable state after N consecutive identical readings.
Provides rising/falling edge detection.

## Why not VSF native?

Checked VSF source (2026-04-29): only USB OTG debounce timer, no generic
software debounce utility.

## API

```c
void debounce_init(debounce_t *d, uint8_t threshold, bool initial_state);
bool debounce_update(debounce_t *d, bool raw);
bool debounce_get(const debounce_t *d);
bool debounce_changed(const debounce_t *d);
bool debounce_rising(const debounce_t *d);
bool debounce_falling(const debounce_t *d);
```

## Test status

- Unit tests: **8 C cases** — init, threshold crossing, bounce rejection, falling edge,
  stable no-change, threshold 1, NULL safety, rapid toggle

## Dependencies

None.
