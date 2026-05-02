# median_filter

**Status: candidate**
**Layer: L0 (primitives)**

Sliding window median filter. Excellent for rejecting impulse noise (spikes)
in sensor readings while preserving edge response. Uses insertion sort on each
update for O(N) per sample. Window sizes 3-31.
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

## How to test

```bash
cd wheels/primitives/median_filter/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Origin

Proactive build for common embedded components (2026-04-29 batch).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `MEDIAN_FILTER_MAX_SIZE` | 31 | Maximum window size (odd recommended) |

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
