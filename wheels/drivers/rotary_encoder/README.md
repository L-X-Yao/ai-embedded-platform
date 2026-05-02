# rotary_encoder

**Status: candidate**
**Layer: L1 (drivers)**

Software quadrature rotary encoder decoder. Feed A/B pin states on each
edge interrupt. Uses lookup table for state transitions. Tracks absolute
position (int32_t) and last direction.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Found KinhoLeung/Rotary (MIT). FSM Gray code decoder; well-understood. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

None (pure software decoder).

## API

```c
void          encoder_init(rotary_encoder_t *enc);
void          encoder_update(rotary_encoder_t *enc, bool a, bool b);
int32_t       encoder_get_position(const rotary_encoder_t *enc);
encoder_dir_t encoder_get_direction(const rotary_encoder_t *enc);
void          encoder_reset(rotary_encoder_t *enc);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/drivers/rotary_encoder/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — init, CW/CCW full cycle, mixed direction, same state,
  reset, NULL safety, 100-step endurance

## API convention deviations

- **`encoder_` prefix instead of `rotary_encoder_`** (api-conventions §1.1): Shortened for readability. All public API uses `encoder_` consistently.

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
