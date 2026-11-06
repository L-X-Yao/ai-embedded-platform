# mpu6050

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for InvenSense MPU-6050 6-axis IMU (3-axis
accelerometer + 3-axis gyroscope). I2C interface injected via callbacks.
Configurable full-scale ranges for both accel and gyro.
## Origin

Proactive build. MPU-6050 is the most widely used low-cost 6-axis IMU in
embedded applications (drones, robots, wearables, motion controllers).


## Why self-written?

Searched GitHub (2026-05-02). Referenced aler9/sensor-imu (MIT) for register sequences and compensation algorithms. Useful for DMP patterns; MPU6050 legacy. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

I2C HAL (injected via `mpu6050_i2c_t` callbacks). No other dependencies.

## API

```c
mpu6050_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_i2c_t *i2c,
                           const mpu6050_cfg_t *cfg);
mpu6050_err_t mpu6050_read_accel(const mpu6050_t *dev, mpu6050_accel_t *out);
mpu6050_err_t mpu6050_read_gyro(const mpu6050_t *dev, mpu6050_gyro_t *out);
mpu6050_err_t mpu6050_reset(mpu6050_t *dev);
```

Output: `mpu6050_accel_t { float ax_g, ay_g, az_g; }` in g units,
`mpu6050_gyro_t { float gx_dps, gy_dps, gz_dps; }` in dps.

## Configuration (compile-time)

None. Accel range (2/4/8/16 g), gyro range (250/500/1000/2000 dps), and
I2C address are runtime-configurable via `mpu6050_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against RM-MPU-6000A-00 Revision 4.0 (03/09/2012), the register map, and PS-MPU-6000A-00 Revision 3.1 (10/24/2011), the product specification. Not run on hardware.

- **Matched:** Addresses, WHO_AM_I, register addresses, DEVICE_RESET and CLKSEL, the range fields and encodings, accel and gyro sensitivities, big-endian data in one burst per sensor.
- **Fixed:** `mpu6050_reset` left the chip asleep with both ranges at 0 while the driver kept its scales, so reads returned `MPU6050_OK` with zeros. It now waits for DEVICE_RESET to clear, wakes the chip and restores both ranges, and returns the new `MPU6050_ERR_TIMEOUT` if the bit never clears. `mpu6050_init` wrote the wake-up straight after DEVICE_RESET with no wait and no read-back. It now polls the bit the same way, as Adafruit's library does, and init and reset both read SLEEP back, so init can return `MPU6050_ERR_TIMEOUT` too. None of this has run on hardware.
- **Still open:** SMPLRT_DIV and CONFIG are never written, so the chip stays at DLPF_CFG 0 with an 8 kHz gyro rate. There is no temperature read, FIFO, DMP, interrupt or self-test support. Accel and gyro are read separately, so the two can come from different samples.
- **Suspected:** Init skips the start-up time of up to 100 ms, which Linux and Adafruit wait out after a reset. The model finishes a reset inside the write and fills the data registers even in sleep.

## Known issues

- No FIFO or DMP support — raw register reads only.
- No temperature readout (available in MPU-6050 but rarely used).
- Init does a device reset; caller must wait ~100ms after init before first read on real hardware.

## How to test

```bash
cd wheels/drivers/mpu6050/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mpu6050
```

23 test cases, among them: init OK, wrong WHO_AM_I, I2C fail on init,
null safety (8 paths), accel 1g on Z axis, negative accel, gyro 1 dps, 16G
accel range scaling, 2000 dps gyro range scaling, accel I2C fail, gyro I2C fail,
reset, alternate address (0x69), config register encoding verification.

## Test status

- Unit tests (sim): PASS (23 C tests, 13 hw_model tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Separate accel/gyro read functions (not combined) to allow selective sampling; output in physical units (g, dps) not raw counts; device reset + PLL clock source set on init; WHO_AM_I check to fail fast on wiring errors; scale factors from datasheet sensitivity tables.

## Responsible human
@user
