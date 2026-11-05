# as5600 — ams AS5600 I2C 12-bit magnetic rotary position sensor

**Status: candidate**
**Layer: L1 (drivers)**

12-bit contactless magnetic rotary position sensor. Fixed I2C address 0x36.
Raw angle register returns 0-4095 mapping to 0-360 degrees. Supports reading
raw angle, degrees, magnet status, and configuring start position / max angle.
## Origin

Proactive build for rotary position sensing applications (knobs, motor feedback, wind vanes).


## Why self-written?

Searched GitHub (2026-05-02). Found raulgotor/ams_as5600 (MIT). Simple chip, implementations converge. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
as5600_err_t as5600_init(as5600_t *dev, const as5600_i2c_t *i2c);
as5600_err_t as5600_read_raw_angle(const as5600_t *dev, uint16_t *raw);
as5600_err_t as5600_read_degrees(const as5600_t *dev, float *degrees);
as5600_err_t as5600_read_status(const as5600_t *dev, uint8_t *status);
as5600_err_t as5600_set_start_position(const as5600_t *dev, uint16_t pos);
as5600_err_t as5600_set_max_angle(const as5600_t *dev, uint16_t angle);
```

## Configuration (compile-time)

None. All configuration is runtime via API calls.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against the ams AS5600 Datasheet [v1-06] 2018-Jun-20. Not run on hardware.

- **Matched:** Address 0x36, the ZPOS, MANG, CONF, STATUS and RAW ANGLE addresses, STATUS bits, the 12-bit layout and byte order, the 360/4096 scale.
- **Fixed:** `as5600_set_start_position` and `as5600_set_max_angle` overwrote blank bits 7:4 of 0x01 and 0x05, which may hold factory settings; they now read first and keep them. `as5600_set_max_angle` accepted 1 to 204, a range of 18 degrees or less.
- **Still open:** Angle reads use RAW ANGLE, so ZPOS and MANG reach the OUT pin but never a value the driver returns. No CONF, MPOS, ZMCO, AGC or MAGNITUDE access. No burn commands, so ZPOS and MANG are lost at power-down. Init and angle reads ignore MD, so with no magnet they return OK and the caller must check `as5600_read_status`. The model does not rescale ANGLE and has no ZMCO or BURN behaviour.
- **Suspected:** MANG may be writable only while ZMCO is 00, so a burned part could return OK and change nothing. The setters do not wait the 1 ms before a setting takes effect. Init does not wait the 10 ms power-up time.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/as5600/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_as5600
```

## Test status

- Unit tests (sim): 24 tests, all pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Fixed address (no addr field in struct), 12-bit angle mask on read, separate raw/degrees API

## Responsible human

@user
