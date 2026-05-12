# Motion Control (Advanced) -- Integration Recipe

Demonstrates six L1 driver wheels working together as a multi-actuator
motion control system with IMU feedback. Runs on host with mock HAL.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L1 | `mpu6050` | 6-axis IMU (accel + gyro) for tilt sensing |
| L1 | `servo` | PWM servo positioned by tilt angle |
| L1 | `stepper` | Stepper motor with trapezoidal acceleration |
| L1 | `pca9685` | 16-channel I2C PWM driver |
| L1 | `drv8833` | Dual H-bridge DC motor driver |
| L1 | `drv2605` | Haptic driver for vibration feedback |

## Build and Run

```bash
cd examples/motion_control_adv
cmake -GNinja -B build && ninja -C build && ./build/motion_control_adv
```

## Data Flow

```
  mpu6050 (tilt) ──> servo (position by angle)
       │              pca9685 (PWM channels)
       │              stepper (target position)
       │              drv8833 (DC motor direction)
       └──> drv2605 (haptic feedback on completion)
```
