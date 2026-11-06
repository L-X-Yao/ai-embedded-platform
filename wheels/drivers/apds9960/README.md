# apds9960 — I2C proximity, ambient light, RGB color, and gesture sensor

**Status: candidate**
**Layer: L1 (drivers)**

Broadcom/Avago APDS-9960 driver. Supports proximity detection (8-bit), RGBC
color sensing (16-bit per channel), and simple 4-directional gesture recognition
(UP/DOWN/LEFT/RIGHT) via FIFO photodiode data. Fixed I2C address 0x39.
## Origin

Proactive build for proximity/gesture/color sensing use cases (e.g., touchless
UI, ambient light adaptation).


## Why self-written?

Searched GitHub (2026-05-02). Found libdriver/apds9960 (MIT). Not clearly better; most options Arduino C++. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
apds9960_err_t apds9960_init(apds9960_t *dev, const apds9960_i2c_t *i2c);
apds9960_err_t apds9960_enable_proximity(apds9960_t *dev);
apds9960_err_t apds9960_read_proximity(apds9960_t *dev, uint8_t *prox);
apds9960_err_t apds9960_enable_color(apds9960_t *dev);
apds9960_err_t apds9960_read_color(apds9960_t *dev, apds9960_color_t *color);
apds9960_err_t apds9960_enable_gesture(apds9960_t *dev);
apds9960_err_t apds9960_read_gesture(apds9960_t *dev, apds9960_gesture_t *gesture);
apds9960_err_t apds9960_disable_all(apds9960_t *dev);
```

## Configuration (compile-time)

None. All configuration is handled at runtime via the API.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Datasheet check

Checked on 2026-11-06 against Avago APDS-9960 data sheet, AV02-4191EN, November 8, 2013. Not run on hardware.

- **Matched:** I2C address, ID 0xAB, all register addresses, ENABLE bits, ATIME, CONTROL, GCONF2 gain and drive, the RGBC block read, gesture FIFO order.
- **Fixed:** `apds9960_init` wrote PPULSE 0x87, which is 16 us pulses, under a comment saying 8 us. It now writes 0x47, 8 pulses of 8 us as in the manual's test conditions, so proximity counts are lower at the same distance. The GCONF1, GCONF2 and GCONF4 comments described bits the written values do not set; the values are unchanged. The model fills data only from cycles of enabled engines, sets AVALID and PVALID, and holds 32 gesture datasets with page reads. `apds9960_enable_gesture` never wrote GPULSE and set 1x gain, so gesture ran on one 8 us pulse. It now writes GPULSE 0xC9 and GGAIN 4x, the values of SparkFun's and Adafruit's libraries, which may need tuning on the real optics.
- **Still open:** `apds9960_read_gesture` sums U-D and L-R and maps the sign to a direction, but the manual has the same diode larger on entry and smaller on exit, so a whole swipe sums to about zero and parts of it read as opposite directions. Expect wrong or missing gestures on hardware. This is not fixed: it needs datasets collected across calls until GMODE returns to 0, and the sign needs hardware because the p17 text and Figure 11 disagree. GFOV and the saturation flags are never read, and the HAL cannot do the address-only write that clears interrupts. `apds9960_enable_gesture` changes gesture registers while other engines may run, which p20 warns against. `delay_ms` is never called. No thresholds, persistence, WTIME, offsets, GDIMS, LED_BOOST or lux calculation.
- **Suspected:** Reads never check AVALID or PVALID, so an early read may return zeros with `APDS9960_OK`. `apds9960_init` does not wait the 5.7 ms after POR. ID 0xA8 is accepted, which this manual does not list.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/apds9960/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_apds9960
```

## Test status

- Unit tests (sim): 21 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Fixed address (0x39, no config needed). Simple FIFO-based gesture detection using U/D and L/R delta comparison with threshold.

## Responsible human

@user
