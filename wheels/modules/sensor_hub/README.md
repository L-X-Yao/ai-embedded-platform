# sensor_hub -- multi-sensor polling and aggregation

**Status: candidate**
**Layer: L3 (modules)**

Passive multi-sensor polling module. Application registers sensor read
callbacks, then calls `sensor_hub_poll()` periodically. The hub iterates
all registered sensors, reads each via its callback, and automatically
tracks min/max/avg statistics per sensor.

## Why not an open-source library?

Checked common embedded sensor frameworks (2026-05-01):

- **SensorManager (Android)** -- Java/Kotlin, OS-coupled, not for bare-metal
- **Zephyr sensor subsystem** -- deeply tied to Zephyr RTOS device model
- **ESP-IDF sensor drivers** -- coupled to FreeRTOS + ESP-IDF HAL

All violate our wheel contract: zero OS dependency, no malloc, no globals,
callback-injected I/O. A passive poll-based hub with statistics is
straightforward to implement (~120 lines) and matches our platform
conventions exactly.

**Conclusion**: no suitable open-source implementation. Self-contained
wheel with zero dependencies.

## Origin

Proactive build for embedded sensor data acquisition. Common pattern
across projects: multiple sensors polled in a main loop, with aggregated
statistics for logging/display/telemetry.

## Dependencies

None. Pure C99 with `<string.h>`, `<stdint.h>`, `<stddef.h>` only.

## API

```c
/* Read callback signature */
typedef sensor_hub_err_t (*sensor_hub_read_fn)(void *ctx, float *value);

/* Initialize hub context */
sensor_hub_err_t sensor_hub_init(sensor_hub_t *hub);

/* Register a sensor with name, unit, and read callback */
sensor_hub_err_t sensor_hub_register(sensor_hub_t *hub, const char *name,
                                     const char *unit,
                                     sensor_hub_read_fn read, void *ctx);

/* Poll all sensors (call periodically) */
sensor_hub_err_t sensor_hub_poll(sensor_hub_t *hub);

/* Get latest value and status for a sensor by index */
sensor_hub_err_t sensor_hub_get(const sensor_hub_t *hub, uint8_t index,
                                float *value, sensor_hub_status_t *status);

/* Get statistics (min/max/avg) for a sensor by index */
sensor_hub_err_t sensor_hub_get_stats(const sensor_hub_t *hub, uint8_t index,
                                      float *min, float *max, float *avg);

/* Reset statistics for all sensors */
void sensor_hub_reset_stats(sensor_hub_t *hub);

/* Get number of registered sensors */
uint8_t sensor_hub_count(const sensor_hub_t *hub);

/* Find sensor by name, returns index or -1 */
int sensor_hub_find(const sensor_hub_t *hub, const char *name);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `SENSOR_HUB_MAX_SENSORS` | 8 | Max registered sensors |

## Supported platforms

- [x] Host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/modules/sensor_hub/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sensor_hub
```

## Test status

- Unit tests (sim): **PASS** (23 test cases, 0 failures)
- Coverage: init (valid/null), register+poll, max sensors (ERR_FULL),
  poll with sensor error (status/error_count), min/max/avg statistics,
  reset stats, find by name (hit/miss/null), multiple sensors polled,
  NULL parameter validation, invalid index, varying values stats update,
  zero readings avg, sensor count, initial stale status, ctx forwarding,
  error-then-recovery, empty hub poll, partial NULL output pointers,
  unit string storage

## Usage example

```c
#include "sensor_hub.h"

/* Sensor read callbacks -- wrap your driver */
static sensor_hub_err_t read_temp(void *ctx, float *value) {
    (void)ctx;
    return bmp280_read_temp(value) == 0
        ? SENSOR_HUB_OK : SENSOR_HUB_ERR_READ;
}

static sensor_hub_err_t read_hum(void *ctx, float *value) {
    (void)ctx;
    return sht30_read_humidity(value) == 0
        ? SENSOR_HUB_OK : SENSOR_HUB_ERR_READ;
}

/* Setup */
static sensor_hub_t hub;

void sensors_init(void) {
    sensor_hub_init(&hub);
    sensor_hub_register(&hub, "temp", "C", read_temp, NULL);
    sensor_hub_register(&hub, "humidity", "%", read_hum, NULL);
}

/* Call from main loop every 100ms */
void sensors_poll(void) {
    sensor_hub_poll(&hub);

    float val;
    sensor_hub_status_t st;
    for (uint8_t i = 0; i < sensor_hub_count(&hub); i++) {
        sensor_hub_get(&hub, i, &val, &st);
        if (st == SENSOR_HUB_STATUS_OK) {
            log_info("sensor[%d] = %.1f", i, val);
        }
    }
}
```

## Design tradeoffs

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| Polling | Application-driven | Passive design, no OS/timer dependency |
| Statistics | Auto min/max/sum | Common need, negligible overhead |
| Values | float | Covers temperature/pressure/humidity; integer sensors cast |
| Registration | Dynamic (runtime) | Sensors register at startup, count varies per project |
| Error handling | Per-sensor status | One failing sensor does not block others |
| Thread safety | Not thread-safe | Caller serializes (typical: single loop context) |
| Max sensors | Compile-time array | No malloc, predictable memory footprint |

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: pending first review

## Responsible human

@user
