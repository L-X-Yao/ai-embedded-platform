# tb6612 -- Toshiba TB6612FNG dual H-bridge motor driver

**Status: candidate**
**Layer: L1 (drivers)**

GPIO+PWM driver for the TB6612FNG dual H-bridge motor driver. Two independent
motor channels (A/B) with direction control via IN1/IN2 pins, speed control
via PWM, and standby mode via STBY pin. Speed range -1000..+1000 permille.

## Origin

Proactive build for motor control projects. TB6612 is a common low-power
dual H-bridge for small DC motors and robotics.

## Why self-written?

Trivial GPIO+PWM logic. Self-written for platform convention consistency.

## Dependencies

- `gpio_hal` + `pwm_hal` (callback injection, no concrete dependency)

## How to test

```bash
cd wheels/drivers/tb6612/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tb6612
```

## Supported platforms

- [x] Host (sim)
- [ ] Real hardware -- not yet verified

## Responsible human

@user