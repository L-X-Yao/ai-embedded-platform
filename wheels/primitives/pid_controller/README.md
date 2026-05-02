# pid_controller

**Status: candidate**
**Layer: L0 (primitives)**

Discrete PID controller with output clamping and integral anti-windup.
Pure C99, no heap, fully reentrant per-context. Suitable for motor control,
temperature regulation, position servo, and any closed-loop control application.
## Origin

Proactive build for embedded control applications. Common pattern across motor
drivers, thermal controllers, and position servos.

## Dependencies

None. Pure C99 with `<stdint.h>` and `<stdbool.h>` only.

## API

```c
pid_controller_err_t pid_controller_init(pid_controller_t *ctx,
                                         const pid_controller_cfg_t *cfg);
pid_controller_err_t pid_controller_reset(pid_controller_t *ctx);
float                pid_controller_update(pid_controller_t *ctx,
                                           float setpoint, float measured, float dt);
pid_controller_err_t pid_controller_set_tunings(pid_controller_t *ctx,
                                                float kp, float ki, float kd);
pid_controller_err_t pid_controller_set_output_limits(pid_controller_t *ctx,
                                                      float min, float max);
```

## Configuration (compile-time)

None. All parameters are runtime-configurable via `pid_controller_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/pid_controller/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pid_controller
```

31 assertions across 18 test cases: init state, null safety (6 paths), P-only
positive/negative, output clamping max/min, integral accumulation, integral
anti-windup positive/negative, derivative first-step zero, derivative step change,
PID combined, reset clears state, set_tunings, set_output_limits, zero dt,
negative dt, convergence simulation, zero error.

## Test status

- Unit tests (sim): PASS (18 C tests, 31 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Standard positional PID (not velocity form); integral anti-windup via clamping (not back-calculation — simpler, sufficient for most embedded use cases); first derivative step outputs zero to avoid startup spike; dt must be positive (returns 0 on invalid).

## Responsible human
@user
