# as5600 — ams AS5600 I2C 12-bit magnetic rotary position sensor

**Status: candidate**
**Layer: L1 (drivers)**

12-bit contactless magnetic rotary position sensor. Fixed I2C address 0x36.
Raw angle register returns 0-4095 mapping to 0-360 degrees. Supports reading
raw angle, degrees, magnet status, and configuring start position / max angle.
## Origin

Proactive build for rotary position sensing applications (knobs, motor feedback, wind vanes).

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

## Known issues

None.

## How to test

```bash
cd wheels/drivers/as5600/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_as5600
```

## Test status

- Unit tests (sim): 19 tests, all pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Fixed address (no addr field in struct), 12-bit angle mask on read, separate raw/degrees API

## Responsible human

@user
