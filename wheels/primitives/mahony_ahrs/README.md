# mahony_ahrs — Mahony AHRS complementary filter (6/9-axis)

**Status: candidate**
**Layer: L0 (primitives)**

Attitude and heading reference system using Mahony complementary filter.
Fuses accelerometer + gyroscope (6-axis) and optionally magnetometer (9-axis).
Outputs quaternion and Euler angles. Zero hardware dependency — pure math.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`ahrs`, `mahony`, `quaternion`, `imu.*filter`, `attitude` — zero results.

VSF has no attitude estimation or sensor fusion.

## Origin

Extracted from `projects/flybox/src/app/imu_task.c` (Mahony filter for QMI8658A + QMC5883P).
Decoupled: task scheduling, sensor drivers, motion detection stay in the project; pure filter algorithm moves to this wheel.

## API

```c
mahony_t f;
mahony_cfg_t cfg = mahony_default_cfg();  /* Kp=2.0, Ki=0.005, 200Hz */
mahony_init(&f, &cfg);

/* In sensor loop (e.g., 200 Hz): */
mahony_update(&f, gx_dps, gy_dps, gz_dps,  /* gyro deg/s */
                  ax, ay, az,                /* accel (G, normalized) */
                  mx, my, mz);              /* mag (pass 0,0,0 for 6-axis) */

mahony_euler_t euler;
mahony_get_euler(&f, &euler);
printf("roll=%.1f pitch=%.1f yaw=%.1f\n",
       euler.roll_deg, euler.pitch_deg, euler.yaw_deg);
```

## Dependencies

None. Pure C99 + libm (`sqrtf`, `atan2f`, `asinf`, `fabsf`, `copysignf`).

## Configuration

Tuning via `mahony_cfg_t`:

| Field | Default | Description |
|-------|---------|-------------|
| `kp` | 2.0 | Proportional gain (accel/mag correction strength) |
| `ki` | 0.005 | Integral gain (gyro bias estimation rate) |
| `sample_freq_hz` | 200.0 | Update rate in Hz |

## Supported platforms

- [x] Host (sim) — passed 2026-04-28
- [ ] GD32F405 (flybox) — original code verified in flight, wheel not separately HIL tested

## Known issues

- Single-precision float only (~7 digits). Sufficient for consumer IMU but not survey-grade.
- Gyro input must be in deg/s (converted internally to rad/s).
- No automatic sample rate detection — caller must match `sample_freq_hz` to actual loop rate.
- Gimbal lock at ±90° pitch (inherent Euler angle limitation, quaternion output unaffected).

## Tests

```bash
cd wheels/primitives/mahony_ahrs/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mahony_ahrs
```

29 assertions across 13 test cases: default config, identity quaternion, zero Euler at init, level accel convergence, tilted accel (~30° pitch), gyro-only integration (90°/s for 1s), null safety, re-init reset, quaternion normalization after 5000 noisy updates, zero-accel skip, 6-axis vs 9-axis comparison, zero-frequency guard (no NaN), negative-frequency guard.

## Test status

- Unit tests (sim): PASS (13 C tests, 29 assertions)
- HIL: N/A

## API convention deviations

- Init takes `mahony_cfg_t` by pointer (not individual params) — single struct preferred for 3+ tuning parameters.
- No error return from `mahony_update()` — void return is standard for real-time filter loops where checking errors on every sample adds overhead.

## API review

- **Date**: 2026-04-28
- **Key decisions**: State in struct (not globals) for multi-instance; gyro input in deg/s (matches typical IMU output); mag input optional (zeros = 6-axis fallback); Kp/Ki/freq as config struct; Euler extraction separate from update for callers who only need quaternion.

## Responsible human
@user
