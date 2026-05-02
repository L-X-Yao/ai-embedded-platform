// SPDX-License-Identifier: MIT
/*
 * sensor_hub -- multi-sensor polling and aggregation module
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 *
 * Passive design: application calls sensor_hub_poll() periodically,
 * hub reads all registered sensors via callbacks.  Auto min/max/avg
 * statistics tracked per sensor.
 */

#ifndef SENSOR_HUB_H
#define SENSOR_HUB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

#ifndef SENSOR_HUB_MAX_SENSORS
#define SENSOR_HUB_MAX_SENSORS 8
#endif

/* ---------- Error codes ---------- */

typedef enum {
    SENSOR_HUB_OK         =  0,
    SENSOR_HUB_ERR_PARAM = -1,
    SENSOR_HUB_ERR_FULL   = -2,
    SENSOR_HUB_ERR_READ   = -3,
} sensor_hub_err_t;

/* ---------- Sensor status ---------- */

typedef enum {
    SENSOR_HUB_STATUS_OK    = 0,
    SENSOR_HUB_STATUS_ERROR,
    SENSOR_HUB_STATUS_STALE,
} sensor_hub_status_t;

/* ---------- Read callback ---------- */

/** @thread_safe: not thread-safe, caller serializes */
typedef sensor_hub_err_t (*sensor_hub_read_fn)(void *ctx, float *value);

/* ---------- Per-sensor entry (internal) ---------- */

typedef struct {
    const char           *name;         /* sensor name (borrowed ptr) */
    const char           *unit;         /* unit string (borrowed ptr) */
    sensor_hub_read_fn    read;         /* read callback */
    void                 *ctx;          /* callback context */
    float                 value;        /* latest reading */
    float                 min;          /* min since last reset */
    float                 max;          /* max since last reset */
    float                 sum;          /* running sum for avg */
    uint32_t              count;        /* successful read count */
    uint32_t              error_count;  /* failed read count */
    sensor_hub_status_t   status;       /* last poll status */
} sensor_hub_entry_t;

/* ---------- Hub context ---------- */

typedef struct {
    sensor_hub_entry_t  sensors[SENSOR_HUB_MAX_SENSORS];
    uint8_t             sensor_count;   /* number of registered sensors */
} sensor_hub_t;

/* ---------- API ---------- */

/** @thread_safe: not thread-safe, caller serializes
 *  Initialize hub context. Must be called before any other API. */
sensor_hub_err_t sensor_hub_init(sensor_hub_t *hub);

/** @thread_safe: not thread-safe, caller serializes
 *  Register a sensor. Returns ERR_FULL if table is full. */
sensor_hub_err_t sensor_hub_register(sensor_hub_t *hub, const char *name,
                                     const char *unit,
                                     sensor_hub_read_fn read, void *ctx);

/** @thread_safe: not thread-safe, caller serializes
 *  Poll all sensors (call periodically from application loop). */
sensor_hub_err_t sensor_hub_poll(sensor_hub_t *hub);

/** @thread_safe: not thread-safe, caller serializes
 *  Get latest value and status for a sensor by index. */
sensor_hub_err_t sensor_hub_get(const sensor_hub_t *hub, uint8_t index,
                                float *value, sensor_hub_status_t *status);

/** @thread_safe: not thread-safe, caller serializes
 *  Get statistics (min/max/avg) for a sensor by index. */
sensor_hub_err_t sensor_hub_get_stats(const sensor_hub_t *hub, uint8_t index,
                                      float *min, float *max, float *avg);

/** @thread_safe: not thread-safe, caller serializes
 *  Reset statistics for all sensors. */
void sensor_hub_reset_stats(sensor_hub_t *hub);

/** @thread_safe: not thread-safe, caller serializes
 *  Get number of registered sensors. */
uint8_t sensor_hub_count(const sensor_hub_t *hub);

/** @thread_safe: not thread-safe, caller serializes
 *  Find sensor by name. Returns index (0..N-1) or -1 if not found. */
int sensor_hub_find(const sensor_hub_t *hub, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_HUB_H */
