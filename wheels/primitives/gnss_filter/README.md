# gnss_filter

**Status: candidate**
**Layer: L0 (primitives)**

GNSS position EMA filter with satellite/DOP quality gate and ZUPT (Zero
Velocity Update). Smooths lat/lon/alt using exponential moving average,
tightens filtering when IMU reports stationary, and gates output until
quality thresholds hold for a configurable duration.

## Why not VSF native?

VSF has no GNSS position filtering primitive. This wheel provides a
transport-independent filter usable with any NMEA parser or GNSS module driver.

## API

```c
#include "gnss_filter.h"

gnss_filter_err_t gnss_filter_init(gnss_filter_t *flt, const gnss_filter_cfg_t *cfg);
gnss_filter_err_t gnss_filter_update(gnss_filter_t *flt, const gnss_filter_input_t *input,
                                     uint16_t imu_motion_mg, uint32_t now_ms);
gnss_filter_err_t gnss_filter_reset(gnss_filter_t *flt);

bool  gnss_filter_is_ready(const gnss_filter_t *flt);
float gnss_filter_lat(const gnss_filter_t *flt);
float gnss_filter_lon(const gnss_filter_t *flt);
float gnss_filter_alt(const gnss_filter_t *flt);

int64_t gnss_datetime_to_unix(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t min, uint8_t sec);
```

## Usage

```c
gnss_filter_t flt;
gnss_filter_cfg_t cfg = {
    .min_satellites = 6, .max_hdop = 2.0f, .stable_ms = 5000,
    .static_alpha = 0.1f, .strong_alpha = 0.02f,
    .static_speed_cms = 50, .zupt_motion_mg = 100,
};
gnss_filter_init(&flt, &cfg);

gnss_filter_input_t in = { .latitude = 31.23f, .longitude = 121.47f,
                           .satellites = 8, .hdop = 1.2f, .fix_valid = true };
gnss_filter_update(&flt, &in, imu_mg, tick_ms());

if (gnss_filter_is_ready(&flt)) {
    float lat = gnss_filter_lat(&flt);
}
```

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
