// SPDX-License-Identifier: MIT
/*
 * sk6812 — WorldSemi SK6812 RGBW addressable LED driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "sk6812.h"
#include "contract.h"
#include <string.h>

/* Reset time: >80us per SK6812 datasheet */
#define RESET_DELAY_US  100U

/* Bytes per pixel (GRBW) */
#define BYTES_PER_PIXEL  4U

/* ---------- public API ---------- */

sk6812_err_t sk6812_init(sk6812_t *dev, const sk6812_hal_t *hal,
                         uint16_t num_leds)
{
    if (!dev || !hal) return SK6812_ERR_PARAM;
    if (!hal->send_data || !hal->delay_us) return SK6812_ERR_PARAM;
    if (num_leds == 0) return SK6812_ERR_PARAM;

    dev->hal = hal;
    dev->num_leds = num_leds;

    INVARIANT(dev->hal != NULL);
    return SK6812_OK;
}

sk6812_err_t sk6812_set_pixel(sk6812_t *dev, sk6812_pixel_t *buf,
                              uint16_t idx,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!dev || !buf) return SK6812_ERR_PARAM;
    if (idx >= dev->num_leds) return SK6812_ERR_PARAM;

    buf[idx].g = g;
    buf[idx].r = r;
    buf[idx].b = b;
    buf[idx].w = w;

    return SK6812_OK;
}

sk6812_err_t sk6812_show(sk6812_t *dev, const sk6812_pixel_t *buf)
{
    bool ok;
    size_t data_len;

    if (!dev || !dev->hal || !buf) return SK6812_ERR_PARAM;

    /* Send raw pixel data (GRBW order, already in struct layout) */
    data_len = (size_t)dev->num_leds * BYTES_PER_PIXEL;
    ok = dev->hal->send_data(dev->hal->ctx, (const uint8_t *)buf, data_len);
    if (!ok) return SK6812_ERR_HAL;

    /* Reset period: >80us low */
    dev->hal->delay_us(dev->hal->ctx, RESET_DELAY_US);

    return SK6812_OK;
}

sk6812_err_t sk6812_fill(sk6812_t *dev, sk6812_pixel_t *buf,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    uint16_t i;

    if (!dev || !buf) return SK6812_ERR_PARAM;

    for (i = 0; i < dev->num_leds; i++) {
        buf[i].g = g;
        buf[i].r = r;
        buf[i].b = b;
        buf[i].w = w;
    }

    return SK6812_OK;
}

sk6812_err_t sk6812_clear(sk6812_t *dev, sk6812_pixel_t *buf)
{
    if (!dev || !buf) return SK6812_ERR_PARAM;

    memset(buf, 0, (size_t)dev->num_leds * sizeof(sk6812_pixel_t));

    return SK6812_OK;
}
