# Industrial Monitor -- Integration Recipe

Demonstrates eight wheels working together as an industrial CAN bus
monitoring system with closed-loop PID control. Runs entirely on the
host with mock I2C and loopback CAN -- no hardware required.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `pid_controller` | Closed-loop PID temperature control |
| L0 | `rate_limiter` | Token-bucket CAN message throttling |
| L0 | `fixed_point` | Q16.16 math for ADC-to-temperature scaling |
| L1 | `ina219` | Bus voltage and current measurement |
| L1 | `ads1115` | 16-bit ADC for analog temperature sensor |
| L2 | `canbus` | CAN 2.0A frame encode/decode with signal packing |
| L3 | `telemetry` | Structured JSON telemetry collection and publish |
| L3 | `task_watchdog` | Control-loop deadline monitoring |

## What It Demonstrates

1. **Mock HAL injection** -- INA219 and ADS1115 each receive mock I2C
   callbacks with pre-planted register values, proving dependency
   injection works without hardware.

2. **Fixed-point sensor scaling** -- ADS1115 millivolt readings are
   converted to temperature using Q16.16 fixed-point arithmetic,
   avoiding floating-point on resource-constrained targets.

3. **PID control loop** -- A discrete PID controller drives a simulated
   plant model, showing setpoint tracking with anti-windup clamping.

4. **Rate-limited CAN transmission** -- A token-bucket rate limiter
   gates CAN frame sends, preventing bus flooding in high-frequency
   control loops.

5. **CAN frame round-trip** -- Sensor data is packed into CAN 2.0A
   frames using bit-level signal packing, serialized to wire format,
   and unpacked to verify lossless round-trip.

6. **Telemetry aggregation** -- All sensor readings and PID output are
   collected into a JSON telemetry packet via the telemetry wheel.

7. **Watchdog supervision** -- A task watchdog monitors the control
   loop, detecting missed deadlines and firing a callback on expiry.

## Build and Run

```bash
cd examples/industrial_monitor
cmake -GNinja -B build && ninja -C build && ./build/industrial_monitor
```

Expected output: all 9 integration tests pass, with tables showing
PID convergence, CAN frame contents, and telemetry JSON output.

## Data Flow

```
  ina219 ----[I2C mock]--> bus voltage -------+
                                              |
  ads1115 ---[I2C mock]--> ADC mV ---[fp]--> temp_c
                                              |
                                              v
                                     pid_controller
                                        |      |
                                        v      v
                               control output  measured
                                        |
                         rate_limiter ---+
                              |
                    [allowed] |  [denied: drop]
                              v
                       canbus_frame_pack
                              |
                              v
                       CAN loopback buffer
                              |
                              v
                     canbus_frame_unpack (verify)

  All values -----> telemetry ----[JSON]--> send callback

  task_watchdog --tick/feed--> monitors control loop deadline
```
