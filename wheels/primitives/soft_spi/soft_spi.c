// SPDX-License-Identifier: MIT
/*
 * soft_spi --- GPIO bit-bang SPI master (modes 0-3, MSB/LSB)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "soft_spi.h"
#include "contract.h"

/* ---------- internal helpers ---------- */

static inline void sck_idle(const soft_spi_gpio_t *g, uint8_t mode)
{
    /* CPOL: modes 0,1 idle low; modes 2,3 idle high */
    if (mode < 2) {
        g->sck_low(g->ctx);
    } else {
        g->sck_high(g->ctx);
    }
}

static inline void mosi_bit(const soft_spi_gpio_t *g, bool bit)
{
    if (bit) {
        g->mosi_high(g->ctx);
    } else {
        g->mosi_low(g->ctx);
    }
}

/*
 * Core single-byte transfer for all 4 SPI modes.
 *
 * Mode 0 (CPOL=0, CPHA=0): data setup on falling edge, sample on rising edge
 * Mode 1 (CPOL=0, CPHA=1): data setup on rising edge, sample on falling edge
 * Mode 2 (CPOL=1, CPHA=0): data setup on rising edge, sample on falling edge
 * Mode 3 (CPOL=1, CPHA=1): data setup on falling edge, sample on rising edge
 *
 * Generalised:
 *   CPHA=0: setup MOSI before first clock edge, sample on leading edge
 *   CPHA=1: setup MOSI on leading edge, sample on trailing edge
 *
 *   CPOL=0: leading edge = rising,  trailing edge = falling
 *   CPOL=1: leading edge = falling, trailing edge = rising
 */
static uint8_t xfer_byte(const soft_spi_gpio_t *g,
                          const soft_spi_cfg_t *cfg,
                          uint8_t tx)
{
    uint8_t rx = 0;
    bool cpol = cfg->mode >= 2;
    bool cpha = (cfg->mode & 1) != 0;

    for (int i = 0; i < 8; i++) {
        int bit_pos = cfg->lsb_first ? i : (7 - i);
        bool tx_bit = (tx >> bit_pos) & 1;

        if (!cpha) {
            /* CPHA=0: setup MOSI before leading edge */
            mosi_bit(g, tx_bit);
            g->delay_us(g->ctx, 0);

            /* Leading edge */
            if (!cpol) {
                g->sck_high(g->ctx);
            } else {
                g->sck_low(g->ctx);
            }
            g->delay_us(g->ctx, 0);

            /* Sample MISO on leading edge */
            if (g->miso_read(g->ctx)) {
                rx |= (uint8_t)(1 << bit_pos);
            }

            /* Trailing edge */
            if (!cpol) {
                g->sck_low(g->ctx);
            } else {
                g->sck_high(g->ctx);
            }
            g->delay_us(g->ctx, 0);
        } else {
            /* CPHA=1: leading edge first, then setup + sample on trailing */

            /* Leading edge */
            if (!cpol) {
                g->sck_high(g->ctx);
            } else {
                g->sck_low(g->ctx);
            }

            /* Setup MOSI on leading edge */
            mosi_bit(g, tx_bit);
            g->delay_us(g->ctx, 0);

            /* Trailing edge */
            if (!cpol) {
                g->sck_low(g->ctx);
            } else {
                g->sck_high(g->ctx);
            }
            g->delay_us(g->ctx, 0);

            /* Sample MISO on trailing edge */
            if (g->miso_read(g->ctx)) {
                rx |= (uint8_t)(1 << bit_pos);
            }
        }
    }

    return rx;
}

/* ---------- public API ---------- */

soft_spi_err_t soft_spi_init(soft_spi_t *dev,
                             const soft_spi_gpio_t *gpio,
                             const soft_spi_cfg_t *cfg)
{
    REQUIRE(dev != NULL);
    REQUIRE(gpio != NULL);
    REQUIRE(cfg != NULL);
    if (dev == NULL || gpio == NULL || cfg == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }
    REQUIRE(gpio->sck_low != NULL);
    REQUIRE(gpio->sck_high != NULL);
    REQUIRE(gpio->mosi_low != NULL);
    REQUIRE(gpio->mosi_high != NULL);
    REQUIRE(gpio->miso_read != NULL);
    REQUIRE(gpio->delay_us != NULL);
    if (gpio->sck_low == NULL || gpio->sck_high == NULL ||
        gpio->mosi_low == NULL || gpio->mosi_high == NULL ||
        gpio->miso_read == NULL || gpio->delay_us == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }
    REQUIRE(cfg->mode <= 3);
    if (cfg->mode > 3) {
        return SOFT_SPI_ERR_PARAM;
    }

    dev->gpio = gpio;
    dev->cfg  = *cfg;

    /* Set SCK to idle level */
    sck_idle(gpio, cfg->mode);
    /* MOSI idle low */
    gpio->mosi_low(gpio->ctx);

    return SOFT_SPI_OK;
}

uint8_t soft_spi_xfer(soft_spi_t *dev, uint8_t tx)
{
    REQUIRE(dev != NULL);
    if (dev == NULL || dev->gpio == NULL) {
        return 0;
    }
    return xfer_byte(dev->gpio, &dev->cfg, tx);
}

soft_spi_err_t soft_spi_write(soft_spi_t *dev,
                              const uint8_t *data, uint16_t len)
{
    REQUIRE(dev != NULL);
    REQUIRE(data != NULL || len == 0);
    if (dev == NULL || dev->gpio == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }
    if (len > 0 && data == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }

    for (uint16_t i = 0; i < len; i++) {
        (void)xfer_byte(dev->gpio, &dev->cfg, data[i]);
    }

    return SOFT_SPI_OK;
}

soft_spi_err_t soft_spi_read(soft_spi_t *dev,
                             uint8_t *buf, uint16_t len)
{
    REQUIRE(dev != NULL);
    REQUIRE(buf != NULL || len == 0);
    if (dev == NULL || dev->gpio == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }
    if (len > 0 && buf == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }

    for (uint16_t i = 0; i < len; i++) {
        buf[i] = xfer_byte(dev->gpio, &dev->cfg, 0xFF);
    }

    return SOFT_SPI_OK;
}

soft_spi_err_t soft_spi_transfer(soft_spi_t *dev,
                                 const uint8_t *tx, uint8_t *rx,
                                 uint16_t len)
{
    REQUIRE(dev != NULL);
    REQUIRE(tx != NULL || len == 0);
    REQUIRE(rx != NULL || len == 0);
    if (dev == NULL || dev->gpio == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }
    if (len > 0 && tx == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }
    if (len > 0 && rx == NULL) {
        return SOFT_SPI_ERR_PARAM;
    }

    for (uint16_t i = 0; i < len; i++) {
        rx[i] = xfer_byte(dev->gpio, &dev->cfg, tx[i]);
    }

    return SOFT_SPI_OK;
}
