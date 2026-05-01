# stepper — step/dir motor driver with trapezoidal acceleration

**Status: candidate**
**Layer: L1 (drivers)**

Drives step/dir stepper motor drivers (A4988, DRV8825, TMC2209, etc.).
Trapezoidal acceleration profile with integer-only math (no float).
Blocking move via caller-provided `delay_us` callback, position tracking.

## Why not VSF native?

VSF has no stepper motor driver. Step/dir control with acceleration ramp is application-specific and not covered by VSF HAL.

## Origin

Proactive build for motor control use cases (CNC, 3D printer, turntable, camera slider).

## Dependencies

None. HAL callbacks (step_pin, dir_pin, delay_us) injected by caller.

## API

```c
stepper_err_t stepper_init(stepper_t *s, const stepper_hal_t *hal,
                           uint16_t max_speed, uint16_t accel);
stepper_err_t stepper_move(stepper_t *s, int32_t steps);
stepper_err_t stepper_move_to(stepper_t *s, int32_t target);
void          stepper_step_once(stepper_t *s, stepper_dir_t dir);
int32_t       stepper_get_position(const stepper_t *s);
void          stepper_set_position(stepper_t *s, int32_t pos);
stepper_state_t stepper_get_state(const stepper_t *s);
```

### HAL struct

```c
typedef struct {
    void (*step_pin)(void *ctx, bool level);   /* pulse high→low = one step */
    void (*dir_pin)(void *ctx, bool level);    /* set before stepping */
    void (*delay_us)(void *ctx, uint32_t us);  /* blocking microsecond delay */
    void *ctx;
} stepper_hal_t;
```

### Error codes

| Code | Meaning |
|------|---------|
| `STEPPER_OK` | Success |
| `STEPPER_ERR_PARAMS` | NULL pointer or zero speed/accel |
| `STEPPER_ERR_BUSY` | Reserved for future non-blocking mode |

## Configuration (compile-time)

No compile-time configuration. All parameters set via `stepper_init()`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/stepper/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_stepper
```

## Test status

- Unit tests (sim): 14 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Blocking move with HAL delay injection; integer-only trapezoidal ramp (David Austin algorithm); step/dir interface for driver IC compatibility.

## Responsible human

@user
