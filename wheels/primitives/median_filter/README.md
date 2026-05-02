# median_filter

**Status: candidate**
**Layer: L0 (primitives)**

Sliding window median filter. Excellent for rejecting impulse noise (spikes)
in sensor readings while preserving edge response. Uses insertion sort on each
update for O(N) per sample. Window sizes 3-31.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Found accabog/MedianFilter (MIT). Trivial sliding-window median algorithm. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

## Dependencies

None.

## API

```c
int     median_filter_init(median_filter_t *f, uint8_t size);
int32_t median_filter_update(median_filter_t *f, int32_t sample);
int32_t median_filter_get(const median_filter_t *f);
void    median_filter_reset(median_filter_t *f);
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `MEDIAN_FILTER_MAX_SIZE` | 31 | Maximum window size (odd recommended) |

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/primitives/median_filter/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **9 C cases** — init valid/invalid, window 3, spike rejection,
  constant signal, descending, negative values, reset, get

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
