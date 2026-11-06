# mpr121 — NXP MPR121 12-channel capacitive touch sensor (I2C)

**Status: candidate**
**Layer: L1 (drivers)**

12 capacitive sensing electrodes with per-channel 10-bit filtered
data readout. Supports configurable touch/release thresholds and
electrode configuration via ECR register. Soft-reset verification
(no chip ID register).

## Origin

Proactive build for capacitive touch input applications (keypads,
proximity sensing, touch panels).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- Adafruit_MPR121: C++/Arduino, heap-heavy, not suitable for bare-metal C99.
- mpr121-driver (various): most are Arduino/C++ or Linux kernel modules.
- No suitable zero-dependency C99 implementation found.

Reference: NXP MPR121 datasheet (AN3944 application note).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
/* Error codes */
typedef enum {
    MPR121_OK        =  0,
    MPR121_ERR_I2C   = -1,
    MPR121_ERR_PARAM = -2,
} mpr121_err_t;

/* Configuration */
typedef struct {
    uint8_t touch_threshold;    /* per-electrode (default 12) */
    uint8_t release_threshold;  /* per-electrode (default 6)  */
    uint8_t ecr;                /* electrode config (default 0x8F) */
} mpr121_cfg_t;

/* Init: soft-reset + configure thresholds + ECR */
mpr121_err_t mpr121_init(mpr121_t *dev, const mpr121_i2c_t *i2c,
                         const mpr121_cfg_t *cfg);

/* Read 12-channel touch bitmap (bit N = electrode N) */
mpr121_err_t mpr121_read_touched(mpr121_t *dev, uint16_t *status);

/* Read 10-bit filtered electrode data for channel [0..11] */
mpr121_err_t mpr121_read_electrode(mpr121_t *dev, uint8_t channel,
                                   uint16_t *value);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `CONTRACT_ENABLED` | off | Enable design-by-contract assertions |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against Freescale MPR121 Technical Data, Rev 4, 09/2010. Not run on hardware.

- **Matched:** Address 0x5A, the touch status, electrode data, filter, threshold, ECR and soft reset registers, configuring in Stop Mode with ECR last.
- **Fixed:** `mpr121_read_touched` returned all 16 status bits, so proximity read as electrode 12 and an over-current fault read as `MPR121_OK`. It now masks the bitmap to 0x0FFF and returns `MPR121_ERR_OVERCURRENT` when OVCF is set. The model now resets 0x5C and 0x5D to 0x10 and 0x24, loses control register writes in Run Mode, measures only there, and clears the output registers on a soft reset.
- **Still open:** The address is fixed at 0x5A, so ADDR strapped to VDD, SDA or SCL, or two parts on one bus, cannot be used. No API clears OVCF: the caller writes it through the HAL. `cfg->ecr` can enable proximity, but its filters and thresholds stay at 0 and no API reports it. Touched filter, debounce, AFE, CDC/CDT and auto-configuration stay at reset values, and baseline values, out-of-range status, GPIO/LED and IRQ are not supported.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/mpr121/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mpr121
./build/test_hw_model_mpr121
```

## Test status

- Unit tests (sim): 19 tests (mock) + 17 tests (hw_model) = 36 total
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: No ERR_ID code (MPR121 has no chip ID register); verification via ECR==0x00 after soft reset. Config struct for thresholds/ECR passed to init.

## Responsible human

@user
