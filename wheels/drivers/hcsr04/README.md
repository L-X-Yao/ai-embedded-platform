# hcsr04

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for HC-SR04 ultrasonic distance sensor.
GPIO trigger + echo timing interface injected via callbacks.
Range 2cm - 400cm, timeout at 25000us (~4.3m).

## Origin

Proactive build. HC-SR04 is the most common ultrasonic distance sensor
in embedded applications (robotics, parking sensors, level measurement).

## Why self-written?

Simple GPIO timing protocol. No register map or complex state machine.
Self-written for zero-dependency wheel convention compliance.

## Dependencies

GPIO + timer HAL (injected via `hcsr04_hal_t` callbacks). No other dependencies.

## API

```c
hcsr04_err_t hcsr04_init(hcsr04_t *dev, const hcsr04_hal_t *hal);
hcsr04_err_t hcsr04_read_cm(hcsr04_t *dev, float *distance_cm);
hcsr04_err_t hcsr04_read_us(hcsr04_t *dev, uint32_t *echo_us);
```

## Configuration (compile-time)

None. All configuration is via the injected HAL.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-12
- [ ] ARM Cortex-M -- not yet verified (uses injected GPIO/timer HAL)

## Known issues

- Blocking measurement: polls echo pin in a loop. Caller must ensure
  the timing HAL provides real microsecond resolution.
- Temperature compensation not implemented (speed of sound varies with
  temperature). Uses 343 m/s (20C nominal).

## How to test

```bash
cd wheels/drivers/hcsr04/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hcsr04
```

## Test status

- Unit tests (sim): PASS (16 tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: Separate trigger/echo pins via HAL callbacks for maximum portability; blocking polling loop for echo measurement; 25000us timeout covers 4.3m max range; range check 2-400cm rejects out-of-spec readings.

## Responsible human
@user
