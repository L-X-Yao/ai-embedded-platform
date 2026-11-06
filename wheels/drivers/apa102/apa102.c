// SPDX-License-Identifier: MIT
/*
 * apa102 — APA102 (DotStar) SPI-based addressable RGB LED driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * SPI frame structure:
 *   Start:  4 bytes of 0x00
 *   LED[i]: [0xE0 | brightness] [blue] [green] [red]
 *   End:    ceil(num_leds / 2) bytes of 0xFF (min 4)
 */

#include "apa102.h"
#include "contract.h"
#include <string.h>

/* Send one byte over SPI (ignoring return) */
static void spi_send(const apa102_t *dev, uint8_t byte)
{
    dev->spi->xfer(dev->spi->ctx, byte);
}

apa102_err_t apa102_init(apa102_t *dev, const apa102_spi_t *spi,
                         uint16_t num_leds)
{
    if (!dev)
        return APA102_ERR_PARAM;

    /* dev->spi stays NULL unless init succeeds: apa102_show refuses a device
     * without it */
    memset(dev, 0, sizeof(*dev));

    if (!spi)
        return APA102_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer)
        return APA102_ERR_PARAM;
    if (num_leds == 0)
        return APA102_ERR_PARAM;

    dev->spi      = spi;
    dev->num_leds = num_leds;

    INVARIANT(dev->spi != NULL);
    return APA102_OK;
}

apa102_err_t apa102_set_pixel(apa102_pixel_t *buf, uint16_t num_leds,
                              uint16_t index, uint8_t r, uint8_t g,
                              uint8_t b, uint8_t brightness)
{
    if (!buf)
        return APA102_ERR_PARAM;
    if (index >= num_leds)
        return APA102_ERR_PARAM;
    if (brightness > 31)
        brightness = 31;

    buf[index].r = r;
    buf[index].g = g;
    buf[index].b = b;
    buf[index].brightness = brightness;
    return APA102_OK;
}

apa102_err_t apa102_show(apa102_t *dev, const apa102_pixel_t *buf)
{
    if (!dev || !dev->spi || !buf)
        return APA102_ERR_PARAM;

    dev->spi->cs_low(dev->spi->ctx);

    /* Start frame: 4 bytes of 0x00 */
    spi_send(dev, 0x00);
    spi_send(dev, 0x00);
    spi_send(dev, 0x00);
    spi_send(dev, 0x00);

    /* LED frames */
    for (uint16_t i = 0; i < dev->num_leds; i++)
    {
        uint8_t br = buf[i].brightness;
        if (br > 31)
            br = 31;
        spi_send(dev, (uint8_t)(0xE0u | br));
        spi_send(dev, buf[i].b);
        spi_send(dev, buf[i].g);
        spi_send(dev, buf[i].r);
    }

    /* End frame: ceil(num_leds / 2) bytes of 0xFF, minimum 4 */
    uint16_t end_bytes = (uint16_t)((dev->num_leds + 1u) / 2u);
    if (end_bytes < 4)
        end_bytes = 4;
    for (uint16_t i = 0; i < end_bytes; i++)
    {
        spi_send(dev, 0xFF);
    }

    dev->spi->cs_high(dev->spi->ctx);
    return APA102_OK;
}

void apa102_clear(apa102_pixel_t *buf, uint16_t num_leds)
{
    if (!buf)
        return;
    memset(buf, 0, (size_t)num_leds * sizeof(apa102_pixel_t));
}
