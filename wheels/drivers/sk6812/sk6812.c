// SPDX-License-Identifier: MIT
/*
 * sk6812 -- WorldSemi SK6812 RGBW addressable LED driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#include "sk6812.h"
#include "contract.h"
#include <string.h>

/* Bytes per LED: G, R, B, W */
#define BYTES_PER_LED 4U

/* ---------- public API ---------- */

sk6812_err_t sk6812_init(sk6812_t *dev, const sk6812_hal_t *hal,
                         uint16_t num_leds)
{
    if (!dev || !hal) return SK6812_ERR_PARAM;
    if (!hal->send_data) return SK6812_ERR_PARAM;
    if (num_leds == 0) return SK6812_ERR_PARAM;

    dev->hal = hal;
    dev->num_leds = num_leds;

    ENSURE(dev->hal != NULL);
    return SK6812_OK;
}

sk6812_err_t sk6812_set_pixel(sk6812_pixel_t *buf, uint16_t num_leds,
                              uint16_t index,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!buf) return SK6812_ERR_PARAM;
    if (index >= num_leds) return SK6812_ERR_PARAM;

    buf[index].g = g;
    buf[index].r = r;
    buf[index].b = b;
    buf[index].w = w;
    return SK6812_OK;
}

sk6812_err_t sk6812_show(const sk6812_t *dev, const sk6812_pixel_t *buf,
                         uint16_t count)
{
    if (!dev || !dev->hal || !dev->hal->send_data) {
        return SK6812_ERR_PARAM;
    }
    if (!buf || count == 0) return SK6812_ERR_PARAM;

    /* The pixel struct is packed as {g,r,b,w} -- exactly the wire order.
     * Cast to raw bytes and send the entire buffer. */
    uint32_t byte_count = (uint32_t)count * BYTES_PER_LED;
    bool ok = dev->hal->send_data(dev->hal->ctx,
                                  (const uint8_t *)buf,
                                  byte_count);
    return ok ? SK6812_OK : SK6812_ERR_HAL;
}

sk6812_err_t sk6812_fill(sk6812_pixel_t *buf, uint16_t num_leds,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!buf || num_leds == 0) return SK6812_ERR_PARAM;

    for (uint16_t i = 0; i < num_leds; i++) {
        buf[i].g = g;
        buf[i].r = r;
        buf[i].b = b;
        buf[i].w = w;
    }
    return SK6812_OK;
}

sk6812_err_t sk6812_clear(sk6812_pixel_t *buf, uint16_t num_leds)
{
    if (!buf || num_leds == 0) return SK6812_ERR_PARAM;
    memset(buf, 0, (size_t)num_leds * sizeof(sk6812_pixel_t));
    return SK6812_OK;
}