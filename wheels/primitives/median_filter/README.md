# median_filter

**STATUS: candidate**
**Layer: L0 (primitives)**

Sliding window median filter. Excellent for rejecting impulse noise (spikes)
in sensor readings while preserving edge response. Uses insertion sort on each
update for O(N) per sample. Window sizes 3-31.

## Why not VSF native?

Checked VSF source (2026-04-29): no median filter in VSF.

## API

```c
int     median_filter_init(median_filter_t *f, uint8_t size);
int32_t median_filter_update(median_filter_t *f, int32_t sample);
int32_t median_filter_get(const median_filter_t *f);
void    median_filter_reset(median_filter_t *f);
```

## Test status

- Unit tests: **9 C cases** — init valid/invalid, window 3, spike rejection,
  constant signal, descending, negative values, reset, get

## Dependencies

None.
