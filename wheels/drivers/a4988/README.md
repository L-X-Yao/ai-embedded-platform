# a4988 -- Allegro A4988 stepper motor driver

**Status: candidate**
**Layer: L1 (drivers)**

GPIO driver for the A4988 stepper motor driver. Step/Dir interface with
microstepping control (full/half/quarter/eighth/sixteenth) via MS1/MS2/MS3
pins. ENABLE pin for power management. Blocking step_n for multi-step moves.

## Origin

Proactive build for stepper motor projects. A4988 is the most common
step/dir stepper driver for 3D printers and CNC machines.

## Why self-written?

Trivial GPIO logic. Self-written for platform convention consistency.

## Dependencies

- `gpio_hal` (callback injection, no concrete dependency)

## How to test

```bash
cd wheels/drivers/a4988/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_a4988
```

## Supported platforms

- [x] Host (sim)
- [ ] Real hardware -- not yet verified

## Responsible human

@user