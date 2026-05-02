// SPDX-License-Identifier: MIT
/*
 * hx711 — GPIO-based 24-bit ADC driver for load cells
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "hx711.h"
#include <stddef.h>

hx711_err_t hx711_init(hx711_t *dev, const hx711_hal_t *hal, hx711_gain_t gain)
{
    if (!dev || !hal)
        return HX711_ERR_PARAM;
    if (!hal->read_dout || !hal->set_sck || !hal->delay_us)
        return HX711_ERR_PARAM;
    if (gain < HX711_GAIN_128 || gain > HX711_GAIN_64)
        return HX711_ERR_PARAM;

    dev->hal    = hal;
    dev->gain   = gain;
    dev->offset = 0;
    dev->scale  = 1.0f;

    /* ensure SCK low (power-up state) */
    hal->set_sck(hal->ctx, false);

    return HX711_OK;
}

bool hx711_is_ready(const hx711_t *dev)
{
    if (!dev || !dev->hal)
        return false;
    return !dev->hal->read_dout(dev->hal->ctx);
}

hx711_err_t hx711_read_raw(hx711_t *dev, int32_t *value)
{
    uint32_t raw = 0;
    uint8_t  total_pulses;
    uint8_t  i;

    if (!dev || !value || !dev->hal)
        return HX711_ERR_PARAM;

    /* DOUT must be low to indicate data ready */
    if (dev->hal->read_dout(dev->hal->ctx))
        return HX711_ERR_NOT_READY;

    /* 24 data bits: clock out MSB first */
    for (i = 0; i < 24; i++) {
        dev->hal->set_sck(dev->hal->ctx, true);
        dev->hal->delay_us(dev->hal->ctx, 1);

        raw = (raw << 1) | (dev->hal->read_dout(dev->hal->ctx) ? 1U : 0U);

        dev->hal->set_sck(dev->hal->ctx, false);
        dev->hal->delay_us(dev->hal->ctx, 1);
    }

    /* extra pulses to set gain for NEXT reading:
     *   gain 128 = 1 extra (25 total)
     *   gain  32 = 2 extra (26 total)
     *   gain  64 = 3 extra (27 total)
     */
    total_pulses = (uint8_t)dev->gain;
    for (i = 0; i < total_pulses; i++) {
        dev->hal->set_sck(dev->hal->ctx, true);
        dev->hal->delay_us(dev->hal->ctx, 1);
        dev->hal->set_sck(dev->hal->ctx, false);
        dev->hal->delay_us(dev->hal->ctx, 1);
    }

    /* sign-extend 24-bit to 32-bit */
    if (raw & 0x800000U) {
        *value = (int32_t)(raw | 0xFF000000U);
    } else {
        *value = (int32_t)raw;
    }

    return HX711_OK;
}

hx711_err_t hx711_read(hx711_t *dev, float *value)
{
    int32_t     raw;
    hx711_err_t err;

    if (!dev || !value)
        return HX711_ERR_PARAM;
    if (dev->scale == 0.0f)
        return HX711_ERR_PARAM;

    err = hx711_read_raw(dev, &raw);
    if (err != HX711_OK)
        return err;

    *value = (float)(raw - dev->offset) / dev->scale;
    return HX711_OK;
}

hx711_err_t hx711_tare(hx711_t *dev, uint8_t samples)
{
    int32_t     raw;
    int32_t     sum = 0;
    uint8_t     i;
    hx711_err_t err;

    if (!dev || samples == 0)
        return HX711_ERR_PARAM;

    /* Clamp to avoid int32 sum overflow (24-bit values * 100 fits) */
    if (samples > 100)
        samples = 100;

    for (i = 0; i < samples; i++) {
        err = hx711_read_raw(dev, &raw);
        if (err != HX711_OK)
            return err;
        sum += raw;
    }

    dev->offset = sum / (int32_t)samples;
    return HX711_OK;
}

void hx711_set_scale(hx711_t *dev, float scale)
{
    if (dev)
        dev->scale = scale;
}

void hx711_set_offset(hx711_t *dev, int32_t offset)
{
    if (dev)
        dev->offset = offset;
}

void hx711_power_down(hx711_t *dev)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->set_sck(dev->hal->ctx, true);
    dev->hal->delay_us(dev->hal->ctx, 65);  /* >60us triggers power-down */
}

void hx711_power_up(hx711_t *dev)
{
    if (!dev || !dev->hal)
        return;
    dev->hal->set_sck(dev->hal->ctx, false);
}
