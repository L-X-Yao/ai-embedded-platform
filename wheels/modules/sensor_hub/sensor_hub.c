// SPDX-License-Identifier: MIT
/*
 * sensor_hub -- multi-sensor polling and aggregation module
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "sensor_hub.h"
#include "contract.h"
#include <string.h>

sensor_hub_err_t sensor_hub_init(sensor_hub_t *hub)
{
    if (!hub) {
        return SENSOR_HUB_ERR_PARAM;
    }
    memset(hub, 0, sizeof(*hub));
    ENSURE(hub->sensor_count == 0);
    return SENSOR_HUB_OK;
}

sensor_hub_err_t sensor_hub_register(sensor_hub_t *hub, const char *name,
                                     const char *unit,
                                     sensor_hub_read_fn read, void *ctx)
{
    if (!hub || !name || !read) {
        return SENSOR_HUB_ERR_PARAM;
    }
    if (hub->sensor_count >= SENSOR_HUB_MAX_SENSORS) {
        return SENSOR_HUB_ERR_FULL;
    }

    sensor_hub_entry_t *e = &hub->sensors[hub->sensor_count];
    memset(e, 0, sizeof(*e));
    e->name   = name;
    e->unit   = unit;
    e->read   = read;
    e->ctx    = ctx;
    e->status = SENSOR_HUB_STATUS_STALE;
    hub->sensor_count++;
    INVARIANT(hub->sensor_count <= SENSOR_HUB_MAX_SENSORS);
    return SENSOR_HUB_OK;
}

sensor_hub_err_t sensor_hub_poll(sensor_hub_t *hub)
{
    if (!hub) {
        return SENSOR_HUB_ERR_PARAM;
    }

    sensor_hub_err_t result = SENSOR_HUB_OK;

    for (uint8_t i = 0; i < hub->sensor_count; i++) {
        sensor_hub_entry_t *e = &hub->sensors[i];
        float val = 0.0f;
        sensor_hub_err_t err = e->read(e->ctx, &val);

        if (err != SENSOR_HUB_OK) {
            e->status = SENSOR_HUB_STATUS_ERROR;
            e->error_count++;
            result = SENSOR_HUB_ERR_READ;
            continue;
        }

        /* reject NaN — treat as read error to avoid poisoning stats */
        if (val != val) {
            e->status = SENSOR_HUB_STATUS_ERROR;
            e->error_count++;
            result = SENSOR_HUB_ERR_READ;
            continue;
        }

        e->value  = val;
        e->status = SENSOR_HUB_STATUS_OK;

        if (e->count == 0) {
            e->min = val;
            e->max = val;
        } else {
            if (val < e->min) { e->min = val; }
            if (val > e->max) { e->max = val; }
        }
        e->sum += val;
        e->count++;
    }

    return result;
}

sensor_hub_err_t sensor_hub_get(const sensor_hub_t *hub, uint8_t index,
                                float *value, sensor_hub_status_t *status)
{
    if (!hub || index >= hub->sensor_count) {
        return SENSOR_HUB_ERR_PARAM;
    }

    const sensor_hub_entry_t *e = &hub->sensors[index];
    if (value)  { *value  = e->value; }
    if (status) { *status = e->status; }
    return SENSOR_HUB_OK;
}

sensor_hub_err_t sensor_hub_get_stats(const sensor_hub_t *hub, uint8_t index,
                                      float *min, float *max, float *avg)
{
    if (!hub || index >= hub->sensor_count) {
        return SENSOR_HUB_ERR_PARAM;
    }

    const sensor_hub_entry_t *e = &hub->sensors[index];
    if (min) { *min = e->min; }
    if (max) { *max = e->max; }
    if (avg) {
        *avg = (e->count > 0) ? (e->sum / (float)e->count) : 0.0f;
    }
    return SENSOR_HUB_OK;
}

void sensor_hub_reset_stats(sensor_hub_t *hub)
{
    if (!hub) {
        return;
    }
    for (uint8_t i = 0; i < hub->sensor_count; i++) {
        sensor_hub_entry_t *e = &hub->sensors[i];
        e->min         = 0.0f;
        e->max         = 0.0f;
        e->sum         = 0.0f;
        e->count       = 0;
        e->error_count = 0;
    }
}

uint8_t sensor_hub_count(const sensor_hub_t *hub)
{
    if (!hub) {
        return 0;
    }
    return hub->sensor_count;
}

int sensor_hub_find(const sensor_hub_t *hub, const char *name)
{
    if (!hub || !name) {
        return -1;
    }
    for (uint8_t i = 0; i < hub->sensor_count; i++) {
        if (hub->sensors[i].name && strcmp(hub->sensors[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}
